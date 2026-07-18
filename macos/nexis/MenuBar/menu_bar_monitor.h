#ifndef MENU_BAR_MONITOR_H
#define MENU_BAR_MONITOR_H

#include <QObject>
#include <QList>

struct MemorySnapshot;

// FW-20 (SSO-3748) MVP: optional NSStatusItem showing live CPU + memory
// percentages. Subscribes to DataRefreshService like a page would
// (FR-103 subscriber counting) so it only costs a sample when enabled.
// Owns no UI itself — see menu_bar_status_item.mm for the AppKit bridge.
class MenuBarMonitor : public QObject
{
    Q_OBJECT

public:
    explicit MenuBarMonitor(QObject *parent = nullptr);
    ~MenuBarMonitor() override;

    void setEnabled(bool enabled);
    bool isEnabled() const { return mEnabled; }

signals:
    // Emitted when the user clicks the status item — App brings the main
    // window forward and navigates to the Dashboard.
    void activationRequested();

private slots:
    void onCpuUpdated(const QList<int> &percents, double clockGHz, const QList<double> &loadAvgs);
    void onMemoryUpdated(const MemorySnapshot &snap);

private:
    void updateTitle();
    static void handleNativeClick();

    bool mEnabled = false;
    int mCpuPercent = 0;
    int mMemPercent = 0;
};

#endif // MENU_BAR_MONITOR_H
