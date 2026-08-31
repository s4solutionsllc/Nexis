#include "menu_bar_monitor.h"
#include "menu_bar_status_item.h"

#include <Managers/data_refresh_service.h>
#include <Managers/info_manager.h>
#include <Managers/cleaner_service.h>
#include <Info/memory_info.h>
#include <Info/disk_info.h>
#include <Utils/menu_bar_format_util.h>
#include <Utils/format_util.h>

#include <QtConcurrent>
#include <QPointer>
#include <QMessageBox>

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

    // Backstop: block destruction until a detached clean worker that may
    // still hold a QPointer to *this* has finished, mirroring
    // MaintenanceWizardDialog's destructor contract.
    mCleanFuture.waitForFinished();
}

void MenuBarMonitor::setEnabled(bool enabled)
{
    if (mEnabled == enabled)
        return;
    mEnabled = enabled;

    if (mEnabled) {
        DataRefreshService::ins()->subscribe(DataRefreshService::Signal::Cpu);
        DataRefreshService::ins()->subscribe(DataRefreshService::Signal::Memory);
        DataRefreshService::ins()->subscribe(DataRefreshService::Signal::DiskUsage);
        connect(DataRefreshService::ins(), &DataRefreshService::cpuUpdated,
                this, &MenuBarMonitor::onCpuUpdated);
        connect(DataRefreshService::ins(), &DataRefreshService::memoryUpdated,
                this, &MenuBarMonitor::onMemoryUpdated);
        connect(DataRefreshService::ins(), &DataRefreshService::diskUsageUpdated,
                this, &MenuBarMonitor::onDiskUsageUpdated);

        nexis_menubar_create(&MenuBarMonitor::handleNativeActivate, &MenuBarMonitor::handleNativeClean);
        updateTitle();
    } else {
        disconnect(DataRefreshService::ins(), nullptr, this, nullptr);
        DataRefreshService::ins()->unsubscribe(DataRefreshService::Signal::Cpu);
        DataRefreshService::ins()->unsubscribe(DataRefreshService::Signal::Memory);
        DataRefreshService::ins()->unsubscribe(DataRefreshService::Signal::DiskUsage);

        nexis_menubar_destroy();
    }
}

void MenuBarMonitor::onCpuUpdated(const QList<int> &percents, double clockGHz,
                                   const QList<double> &loadAvgs)
{
    Q_UNUSED(percents)
    Q_UNUSED(clockGHz)

    // Same formula as DashboardPage::onHealthCpuUpdated (health_score's CPU
    // component) — 1-minute load average relative to core count.
    const int coreCount = InfoManager::ins()->getCpuCoreCount();
    const double load1m = loadAvgs.isEmpty() ? 0.0 : loadAvgs.first();
    int score = 100;
    if (coreCount > 0 && load1m > 0) {
        double ratio = load1m / coreCount;
        score = qBound(0, qRound(100.0 * (1.0 - ratio)), 100);
    }
    mHealthCalculator.setCpuScore(score);
    updateTitle();
}

void MenuBarMonitor::onMemoryUpdated(const MemorySnapshot &snap)
{
    // Same formula as DashboardPage::onHealthMemoryUpdated.
    int score = 100;
    if (snap.total > 0)
        score = qBound(0, 100 - (int)(100.0 * snap.used / snap.total), 100);
    mHealthCalculator.setMemoryScore(score);
    updateTitle();
}

void MenuBarMonitor::onDiskUsageUpdated(const QList<Disk> &disks)
{
    // Same formula as DashboardPage::onHealthDiskUpdated — capacity-weighted
    // average of per-disk free space.
    qint64 totalSize = 0;
    double weightedScore = 0.0;
    for (const Disk &d : disks) {
        if (d.size == 0) continue;
        int usedPercent = (int)(100.0 * d.used / d.size);
        int diskScore = qBound(0, 100 - usedPercent, 100);
        weightedScore += (double)diskScore * d.size;
        totalSize += d.size;
    }
    const int score = (totalSize > 0) ? qBound(0, (int)qRound(weightedScore / totalSize), 100) : 100;
    mHealthCalculator.setDiskScore(score);
    updateTitle();
}

void MenuBarMonitor::updateTitle()
{
    // SSO-23853: temp/battery/SMART stay unavailable here (HealthScoreCalculator
    // defaults them to false) — polling thermal sensors and disk health from
    // a background monitor that ticks continuously is out of scope for this
    // issue. The CPU/memory/disk weights (60% of the Dashboard composite) are
    // renormalized to 100%, so the score matches the Dashboard tile exactly
    // on hardware without those sensors and closely approximates it elsewhere.
    const int score = mHealthCalculator.compositeScore();
    const QString label = mHealthCalculator.scoreLabel();
    const QString title = MenuBarFormatUtil::formatHealthTitle(score, label);
    nexis_menubar_set_title(title.toUtf8().constData());
}

void MenuBarMonitor::handleNativeActivate()
{
    if (gInstance)
        emit gInstance->activationRequested();
}

void MenuBarMonitor::handleNativeClean()
{
    if (gInstance)
        gInstance->startClean();
}

void MenuBarMonitor::startClean()
{
    // SSO-23853: reuses CleanerService (same singleton/category set as the
    // Maintenance Wizard's "Clean Safe Items") off the main thread, following
    // MaintenanceWizardDialog::runChecks()'s QPointer + QtConcurrent pattern
    // since MenuBarMonitor isn't a QWidget and outlives no particular dialog.
    if (mCleaning)
        return;
    mCleaning = true;
    nexis_menubar_set_clean_item_state("Cleaning…", 0);

    QPointer<MenuBarMonitor> self(this);
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

void MenuBarMonitor::onCleanFinished(quint64 bytesFreed, int filesRemoved)
{
    mCleaning = false;
    nexis_menubar_set_clean_item_state("Clean Now", 1);

    // design.md: dialogs stay to one sentence, never a silent no-op.
    const QString message = (bytesFreed > 0)
        ? tr("Freed %1 across %n item(s).", "", filesRemoved).arg(FormatUtil::formatBytes(bytesFreed))
        : tr("Nothing to clean — your system is already tidy.");

    QMessageBox::information(nullptr, tr("Nexis"), message);
}
