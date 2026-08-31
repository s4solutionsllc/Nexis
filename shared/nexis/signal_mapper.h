#ifndef SIGNAL_MAPPER_H
#define SIGNAL_MAPPER_H

#include <QObject>

class SignalMapper : public QObject
{
    Q_OBJECT

public:
    static SignalMapper *ins();

signals:
    void sigChangedAppTheme();
    void sigUninstallStarted();
    void sigUninstallFinished();
    void sigKioskToggleRequested();
    void sigKioskModeChanged(bool enabled);
    void sigAppVisibilityChanged(bool visible);
    // FR-105: emitted when the main window gains/loses focus (QEvent::WindowActivate
    // / WindowDeactivate). Used by DataRefreshService to downshift cadence when
    // the user is working in another app.
    void sigAppFocusChanged(bool focused);
    void sigNavigateToPage(const QString &pageTitle);
    void sigCleanableSizeChanged(quint64 bytes);
    void sigDashboardFooterChanged(bool visible);
    void sigMenuBarMonitorToggled(bool enabled);
    void sigTrayHealthScoreToggled(bool enabled);

private:
    static SignalMapper *instance;

};

#endif // SIGNAL_MAPPER_H
