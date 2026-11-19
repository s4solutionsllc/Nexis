#ifndef RESOURCE_ALERT_MONITOR_H
#define RESOURCE_ALERT_MONITOR_H

#include <QObject>
#include <QElapsedTimer>
#include <QList>

#include <Utils/sustained_threshold_tracker.h>

class DataRefreshService;
class InfoManager;
class SettingManager;
struct MemorySnapshot;

// SSO-27148: tray alerts for CPU %, memory % and hottest-sensor temperature
// that fire only once the metric has stayed above its threshold for the
// configured number of seconds (0 = fire immediately), then stay quiet for
// kCooldownMs. Owns no UI: App turns alertTriggered() into a tray balloon.
//
// Samples come from DataRefreshService, so — like every other consumer —
// evaluation pauses while the window is hidden. The tracker treats the
// resulting sample gap as a restart rather than as time spent above the
// threshold, so a long hidden period can't fabricate a "sustained" alert.
class ResourceAlertMonitor : public QObject
{
    Q_OBJECT

public:
    static constexpr qint64 kCooldownMs = 10 * 60 * 1000;

    explicit ResourceAlertMonitor(QObject *parent = nullptr,
                                  DataRefreshService *refresh = nullptr,
                                  InfoManager *infoManager = nullptr,
                                  SettingManager *settings = nullptr);

    // Re-read thresholds/durations from SettingManager and adjust which
    // DataRefreshService signals are subscribed.
    void reloadSettings();

signals:
    void alertTriggered(const QString &title, const QString &message);

private slots:
    void onCpuUpdated(const QList<int> &percents);
    void onMemoryUpdated(const MemorySnapshot &snap);
    void onTempUpdated();

private:
    enum Metric { Cpu, Memory, Temp, MetricCount };

    void evaluate(Metric metric, double value);
    void applySubscription(Metric metric, bool wanted);

    DataRefreshService *mRefresh;
    InfoManager *im;
    SettingManager *mSettings;
    QElapsedTimer mClock;
    SustainedThresholdTracker mTrackers[MetricCount];
    int mThresholds[MetricCount] = {};
    int mDurationsSecs[MetricCount] = {};
    bool mSubscribed[MetricCount] = {};
};

#endif // RESOURCE_ALERT_MONITOR_H
