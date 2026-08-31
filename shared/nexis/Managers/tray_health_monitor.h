#ifndef TRAY_HEALTH_MONITOR_H
#define TRAY_HEALTH_MONITOR_H

#include <QObject>
#include <QList>
#include <QFuture>

#include "health_score_calculator.h"

struct MemorySnapshot;
struct Disk;

// SSO-23854: Linux/QSystemTrayIcon counterpart to macos/nexis/MenuBar/MenuBarMonitor
// (SSO-23853) — same HealthScoreCalculator + CleanerService::safeCategories()
// reuse, same DataRefreshService subscription pattern, off by default. Owns no
// UI itself; App wires the emitted text into a QAction label / tray tooltip
// and connects a "Clean Now" QAction's triggered() to startClean().
class TrayHealthMonitor : public QObject
{
    Q_OBJECT

public:
    explicit TrayHealthMonitor(QObject *parent = nullptr);
    ~TrayHealthMonitor() override;

    void setEnabled(bool enabled);
    bool isEnabled() const { return mEnabled; }

signals:
    // "Health 82 · Excellent" — same MenuBarFormatUtil::formatHealthTitle()
    // string macOS shows in its NSStatusItem title and the Dashboard tile.
    void scoreTextChanged(const QString &text);
    // Drives the "Clean Now" QAction's label/enabled state while a clean runs.
    void cleanStateChanged(const QString &label, bool enabled);

public slots:
    void startClean();

private slots:
    void onCpuUpdated(const QList<int> &percents, double clockGHz, const QList<double> &loadAvgs);
    void onMemoryUpdated(const MemorySnapshot &snap);
    void onDiskUsageUpdated(const QList<Disk> &disks);

private:
    void updateScoreText();
    void onCleanFinished(quint64 bytesFreed, int filesRemoved);

    bool mEnabled = false;
    bool mCleaning = false;
    HealthScoreCalculator mHealthCalculator;
    QFuture<void> mCleanFuture;
};

#endif // TRAY_HEALTH_MONITOR_H
