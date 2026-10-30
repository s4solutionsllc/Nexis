#ifndef TRAY_HEALTH_MONITOR_H
#define TRAY_HEALTH_MONITOR_H

#include <QObject>
#include <QList>
#include <QFuture>

#include "health_score_calculator.h"

struct MemorySnapshot;
struct Disk;

// SSO-23854, unified cross-platform in SSO-26762: the QSystemTrayIcon health
// score + Clean Now surface for both Linux and macOS — the macOS-only
// NSStatusItem (MenuBarMonitor) this originally paralleled was retired once
// one QSystemTrayIcon surface replaced both. Same HealthScoreCalculator +
// CleanerService::safeCategories() reuse, same DataRefreshService
// subscription pattern, off by default. Owns no UI itself; App wires the
// emitted text into a QAction label / tray tooltip, connects a "Clean Now"
// QAction's triggered() to startClean(), and decides how to surface
// cleanCompleted() (dialog vs. tray balloon) based on window visibility.
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
    // SSO-26762 requirement 9: the one-sentence clean-result message. App
    // decides how to surface it (parented dialog vs. tray balloon) based on
    // main-window visibility — this class owns no UI and must not show a
    // parentless QMessageBox itself (crash risk while hidden on macOS).
    void cleanCompleted(const QString &message);

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
