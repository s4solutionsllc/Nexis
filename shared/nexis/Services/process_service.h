#ifndef PROCESS_SERVICE_H
#define PROCESS_SERVICE_H

#include <QObject>

class ProcessService : public QObject
{
    Q_OBJECT

public:
    static ProcessService *ins();

    bool killProcess(pid_t pid, const QString &processUser, const QString &currentUser);

signals:
    void processKilled(pid_t pid);
    void processKillFailed(pid_t pid, QString error);

private:
    explicit ProcessService(QObject *parent = nullptr);
    static ProcessService *instance;
};

#endif // PROCESS_SERVICE_H
