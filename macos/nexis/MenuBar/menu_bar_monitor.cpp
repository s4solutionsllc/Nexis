#include "menu_bar_monitor.h"
#include "menu_bar_status_item.h"

#include <Managers/data_refresh_service.h>
#include <Info/memory_info.h>
#include <Utils/menu_bar_format_util.h>

namespace {
MenuBarMonitor *gInstance = nullptr;
}

MenuBarMonitor::MenuBarMonitor(QObject *parent) : QObject(parent)
{
    gInstance = this;
}

MenuBarMonitor::~MenuBarMonitor()
{
    setEnabled(false);
    if (gInstance == this)
        gInstance = nullptr;
}

void MenuBarMonitor::setEnabled(bool enabled)
{
    if (mEnabled == enabled)
        return;
    mEnabled = enabled;

    if (mEnabled) {
        DataRefreshService::ins()->subscribe(DataRefreshService::Signal::Cpu);
        DataRefreshService::ins()->subscribe(DataRefreshService::Signal::Memory);
        connect(DataRefreshService::ins(), &DataRefreshService::cpuUpdated,
                this, &MenuBarMonitor::onCpuUpdated);
        connect(DataRefreshService::ins(), &DataRefreshService::memoryUpdated,
                this, &MenuBarMonitor::onMemoryUpdated);

        nexis_menubar_create(&MenuBarMonitor::handleNativeClick);
        updateTitle();
    } else {
        disconnect(DataRefreshService::ins(), nullptr, this, nullptr);
        DataRefreshService::ins()->unsubscribe(DataRefreshService::Signal::Cpu);
        DataRefreshService::ins()->unsubscribe(DataRefreshService::Signal::Memory);

        nexis_menubar_destroy();
    }
}

void MenuBarMonitor::onCpuUpdated(const QList<int> &percents, double clockGHz,
                                   const QList<double> &loadAvgs)
{
    Q_UNUSED(clockGHz)
    Q_UNUSED(loadAvgs)

    if (percents.isEmpty())
        return;
    mCpuPercent = percents.at(0);
    updateTitle();
}

void MenuBarMonitor::onMemoryUpdated(const MemorySnapshot &snap)
{
    mMemPercent = snap.total
        ? static_cast<int>((double(snap.used) / double(snap.total)) * 100.0)
        : 0;
    updateTitle();
}

void MenuBarMonitor::updateTitle()
{
    const QString title = MenuBarFormatUtil::formatTitle(mCpuPercent, mMemPercent);
    nexis_menubar_set_title(title.toUtf8().constData());
}

void MenuBarMonitor::handleNativeClick()
{
    if (gInstance)
        emit gInstance->activationRequested();
}
