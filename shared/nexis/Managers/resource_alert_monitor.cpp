#include "resource_alert_monitor.h"

#include <Managers/data_refresh_service.h>
#include <Managers/info_manager.h>
#include <Managers/setting_manager.h>
#include <Info/memory_info.h>

ResourceAlertMonitor::ResourceAlertMonitor(QObject *parent, DataRefreshService *refresh,
                                           InfoManager *infoManager, SettingManager *settings)
    : QObject(parent),
      mRefresh(refresh ? refresh : DataRefreshService::ins()),
      im(infoManager ? infoManager : InfoManager::ins()),
      mSettings(settings ? settings : SettingManager::ins())
{
    mClock.start();

    connect(mRefresh, &DataRefreshService::cpuUpdated, this,
            [this](const QList<int> &percents, double, const QList<double> &) { onCpuUpdated(percents); });
    connect(mRefresh, &DataRefreshService::memoryUpdated, this, &ResourceAlertMonitor::onMemoryUpdated);
    connect(mRefresh, &DataRefreshService::tempUpdated, this, &ResourceAlertMonitor::onTempUpdated);

    reloadSettings();
}

void ResourceAlertMonitor::reloadSettings()
{
    mThresholds[Cpu] = mSettings->getCpuAlertPercent();
    mDurationsSecs[Cpu] = mSettings->getCpuAlertSeconds();
    mThresholds[Memory] = mSettings->getMemoryAlertPercent();
    mDurationsSecs[Memory] = mSettings->getMemoryAlertSeconds();
    mThresholds[Temp] = im->hasThermalSensors() ? mSettings->getTempAlertCelsius() : 0;
    mDurationsSecs[Temp] = mSettings->getTempAlertSeconds();

    for (int m = 0; m < MetricCount; ++m) {
        SustainedThresholdTracker::Config config;
        config.threshold = mThresholds[m];
        config.durationMs = qint64(mDurationsSecs[m]) * 1000;
        config.cooldownMs = kCooldownMs;
        mTrackers[m].setConfig(config);
        applySubscription(Metric(m), mThresholds[m] > 0);
    }
}

void ResourceAlertMonitor::applySubscription(Metric metric, bool wanted)
{
    if (mSubscribed[metric] == wanted)
        return;
    mSubscribed[metric] = wanted;

    static const DataRefreshService::Signal kSignals[MetricCount] = {
        DataRefreshService::Signal::Cpu,
        DataRefreshService::Signal::Memory,
        DataRefreshService::Signal::Temp,
    };
    if (wanted)
        mRefresh->subscribe(kSignals[metric]);
    else
        mRefresh->unsubscribe(kSignals[metric]);
}

void ResourceAlertMonitor::onCpuUpdated(const QList<int> &percents)
{
    if (percents.isEmpty())
        return;
    evaluate(Cpu, percents.at(0));
}

void ResourceAlertMonitor::onMemoryUpdated(const MemorySnapshot &snap)
{
    if (snap.total == 0)
        return;
    evaluate(Memory, 100.0 * double(snap.used) / double(snap.total));
}

void ResourceAlertMonitor::onTempUpdated()
{
    double hottest = 0.0;
    const int count = im->getThermalSensors().size();
    for (int i = 0; i < count; ++i)
        hottest = qMax(hottest, im->getThermalTemperature(i));
    if (hottest > 0.0)
        evaluate(Temp, hottest);
}

void ResourceAlertMonitor::evaluate(Metric metric, double value)
{
    if (mThresholds[metric] <= 0)
        return;
    if (!mTrackers[metric].update(value, mClock.elapsed()))
        return;

    const int threshold = mThresholds[metric];
    const int secs = mDurationsSecs[metric];
    QString title;
    QString message;
    switch (metric) {
    case Cpu:
        title = tr("High CPU Usage");
        message = secs > 0 ? tr("CPU usage has stayed over %1% for %2 seconds.").arg(threshold).arg(secs)
                           : tr("The amount of CPU used is over %1%.").arg(threshold);
        break;
    case Memory:
        title = tr("High Memory Usage");
        message = secs > 0 ? tr("Memory usage has stayed over %1% for %2 seconds.").arg(threshold).arg(secs)
                           : tr("The amount of memory used is over %1%.").arg(threshold);
        break;
    case Temp:
        title = tr("High Temperature");
        message = secs > 0 ? tr("A sensor has stayed over %1 °C for %2 seconds.").arg(threshold).arg(secs)
                           : tr("A sensor is over %1 °C.").arg(threshold);
        break;
    default:
        return;
    }
    emit alertTriggered(title, message);
}
