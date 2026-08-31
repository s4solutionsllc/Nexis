#ifndef MINI_MONITOR_WINDOW_H
#define MINI_MONITOR_WINDOW_H

#include <QWidget>
#include <QList>

#include "health_score_calculator.h"

class QLabel;
struct MemorySnapshot;
struct Disk;

// SSO-23855 (Glanceable Surfaces epic): a small, resizable, always-on-top
// window showing the Dashboard health score plus CPU/MEM/DSK usage. Unlike
// MenuBarMonitor (macos/nexis/MenuBar) this lives entirely in shared/nexis —
// it is a QWidget, not a native NSPanel, so the same implementation covers
// both macOS and Linux. Subscribes to DataRefreshService only while shown
// (FR-103 subscriber counting), same pattern as MenuBarMonitor::setEnabled.
class MiniMonitorWindow : public QWidget
{
    Q_OBJECT

public:
    explicit MiniMonitorWindow(QWidget *parent = nullptr);
    ~MiniMonitorWindow() override;

signals:
    // Mirrors the window's actual open/closed state so a Settings checkbox
    // or tray action can stay in sync when the user closes the window
    // directly (native close button) instead of via the toggle.
    void visibilityToggled(bool visible);

protected:
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

private slots:
    void onCpuUpdated(const QList<int> &percents, double clockGHz, const QList<double> &loadAvgs);
    void onMemoryUpdated(const MemorySnapshot &snap);
    void onDiskUsageUpdated(const QList<Disk> &disks);
    void refreshThemeColors();

private:
    void buildLayout();
    void updateScoreDisplay();
    void updateMetricRows();
    void persistGeometry();
    void setSubscribed(bool subscribed);

    HealthScoreCalculator mHealthCalculator;
    bool mSubscribed = false;

    QLabel *mLblScore;
    QLabel *mLblScoreLabel;
    QLabel *mLblCpu;
    QLabel *mLblMem;
    QLabel *mLblDisk;
    QLabel *mDotCpu;
    QLabel *mDotMem;
    QLabel *mDotDisk;

    int mLastCpuPercent = 0;
    int mLastMemPercent = 0;
    int mLastDiskPercent = 0;
};

#endif // MINI_MONITOR_WINDOW_H
