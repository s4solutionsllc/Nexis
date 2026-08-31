#include "tray_health_monitor.h"

#include <Managers/data_refresh_service.h>
#include <Managers/info_manager.h>
#include <Managers/cleaner_service.h>
#include <Info/memory_info.h>
#include <Info/disk_info.h>
#include <Utils/health_score_inputs.h>
#include <Utils/menu_bar_format_util.h>
#include <Utils/format_util.h>

#include <QtConcurrent>
#include <QPointer>
#include <QMessageBox>

TrayHealthMonitor::TrayHealthMonitor(QObject *parent) : QObject(parent)
{
}

TrayHealthMonitor::~TrayHealthMonitor()
{
    setEnabled(false);

    // Backstop: block destruction until a detached clean worker that may
    // still hold a QPointer to *this* has finished, mirroring
    // MenuBarMonitor's destructor contract.
    mCleanFuture.waitForFinished();
}

void TrayHealthMonitor::setEnabled(bool enabled)
{
    if (mEnabled == enabled)
        return;
    mEnabled = enabled;

    if (mEnabled) {
        DataRefreshService::ins()->subscribe(DataRefreshService::Signal::Cpu);
        DataRefreshService::ins()->subscribe(DataRefreshService::Signal::Memory);
        DataRefreshService::ins()->subscribe(DataRefreshService::Signal::DiskUsage);
        connect(DataRefreshService::ins(), &DataRefreshService::cpuUpdated,
                this, &TrayHealthMonitor::onCpuUpdated);
        connect(DataRefreshService::ins(), &DataRefreshService::memoryUpdated,
                this, &TrayHealthMonitor::onMemoryUpdated);
        connect(DataRefreshService::ins(), &DataRefreshService::diskUsageUpdated,
                this, &TrayHealthMonitor::onDiskUsageUpdated);

        updateScoreText();
    } else {
        disconnect(DataRefreshService::ins(), nullptr, this, nullptr);
        DataRefreshService::ins()->unsubscribe(DataRefreshService::Signal::Cpu);
        DataRefreshService::ins()->unsubscribe(DataRefreshService::Signal::Memory);
        DataRefreshService::ins()->unsubscribe(DataRefreshService::Signal::DiskUsage);
    }
}

void TrayHealthMonitor::onCpuUpdated(const QList<int> &percents, double clockGHz,
                                      const QList<double> &loadAvgs)
{
    Q_UNUSED(percents)
    Q_UNUSED(clockGHz)

    const int coreCount = InfoManager::ins()->getCpuCoreCount();
    const double load1m = loadAvgs.isEmpty() ? 0.0 : loadAvgs.first();
    mHealthCalculator.setCpuScore(HealthScoreInputs::cpuScore(coreCount, load1m));
    updateScoreText();
}

void TrayHealthMonitor::onMemoryUpdated(const MemorySnapshot &snap)
{
    mHealthCalculator.setMemoryScore(HealthScoreInputs::memoryScore(snap));
    updateScoreText();
}

void TrayHealthMonitor::onDiskUsageUpdated(const QList<Disk> &disks)
{
    mHealthCalculator.setDiskScore(HealthScoreInputs::diskScore(disks));
    updateScoreText();
}

void TrayHealthMonitor::updateScoreText()
{
    // SSO-23854: same composite/label + format string as the macOS menu-bar
    // surface (MenuBarFormatUtil::formatHealthTitle) and the Dashboard tile —
    // temp/battery/SMART stay unavailable for the same reason MenuBarMonitor
    // leaves them unavailable (see menu_bar_monitor.cpp).
    const int score = mHealthCalculator.compositeScore();
    const QString label = mHealthCalculator.scoreLabel();
    emit scoreTextChanged(MenuBarFormatUtil::formatHealthTitle(score, label));
}

void TrayHealthMonitor::startClean()
{
    // SSO-23854: same CleanerService::safeCategories() set and QtConcurrent +
    // QPointer + invokeMethod pattern as MenuBarMonitor::startClean().
    if (mCleaning)
        return;
    mCleaning = true;
    emit cleanStateChanged(tr("Cleaning…"), false);

    QPointer<TrayHealthMonitor> self(this);
    mCleanFuture = QtConcurrent::run([self]() {
        CleanerService::CleanResult result =
            CleanerService::ins()->clean(CleanerService::safeCategories());
        if (!self) return;
        QMetaObject::invokeMethod(self.data(), [self, result]() {
            if (!self) return;
            self->onCleanFinished(result.totalBytesFreed, result.totalFilesRemoved);
        }, Qt::QueuedConnection);
    });
}

void TrayHealthMonitor::onCleanFinished(quint64 bytesFreed, int filesRemoved)
{
    mCleaning = false;
    emit cleanStateChanged(tr("Clean Now"), true);

    // design.md: dialogs stay to one sentence, never a silent no-op.
    const QString message = (bytesFreed > 0)
        ? tr("Freed %1 across %n item(s).", "", filesRemoved).arg(FormatUtil::formatBytes(bytesFreed))
        : tr("Nothing to clean — your system is already tidy.");

    QMessageBox::information(nullptr, tr("Nexis"), message);
}
