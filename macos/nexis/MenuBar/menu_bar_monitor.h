#ifndef MENU_BAR_MONITOR_H
#define MENU_BAR_MONITOR_H

#include <QObject>
#include <QList>
#include <QFuture>

#include "health_score_calculator.h"

struct MemorySnapshot;
struct Disk;

// FW-20 (SSO-3748) MVP, extended SSO-23853: optional NSStatusItem showing
// the Dashboard health score (was raw CPU/memory percentages) plus a
// one-click "Clean Now" maintenance action. Subscribes to DataRefreshService
// like a page would (FR-103 subscriber counting) so it only costs a sample
// when enabled. Owns no UI itself — see menu_bar_status_item.mm for the
// AppKit bridge (NSStatusItem + NSMenu).
class MenuBarMonitor : public QObject
{
    Q_OBJECT

public:
    explicit MenuBarMonitor(QObject *parent = nullptr);
    ~MenuBarMonitor() override;

    void setEnabled(bool enabled);
    bool isEnabled() const { return mEnabled; }

signals:
    // Emitted when the user picks "Open Nexis" from the status item menu —
    // App brings the main window forward and navigates to the Dashboard.
    void activationRequested();

private slots:
    void onCpuUpdated(const QList<int> &percents, double clockGHz, const QList<double> &loadAvgs);
    void onMemoryUpdated(const MemorySnapshot &snap);
    void onDiskUsageUpdated(const QList<Disk> &disks);

private:
    void updateTitle();
    void startClean();
    void onCleanFinished(quint64 bytesFreed, int filesRemoved);
    static void handleNativeActivate();
    static void handleNativeClean();

    bool mEnabled = false;
    bool mCleaning = false;
    HealthScoreCalculator mHealthCalculator;
    QFuture<void> mCleanFuture;
};

#endif // MENU_BAR_MONITOR_H
