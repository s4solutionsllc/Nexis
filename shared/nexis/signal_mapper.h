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
    void sigScheduledCleanStarted(QString scheduleName);
    void sigScheduledCleanFinished(quint64 bytesFreed, int fileCount);
    void sigKioskToggleRequested();
    void sigKioskModeChanged(bool enabled);
    void sigAppVisibilityChanged(bool visible);

private:
    static SignalMapper *instance;

};

#endif // SIGNAL_MAPPER_H
