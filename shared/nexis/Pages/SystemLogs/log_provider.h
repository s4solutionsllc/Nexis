#ifndef LOG_PROVIDER_H
#define LOG_PROVIDER_H

#include <QObject>
#include <QProcess>
#include <QList>
#include <QDateTime>

struct LogEntry {
    QDateTime timestamp;
    int severity;          // 0=Emergency ... 7=Debug (syslog convention)
    QString unit;          // systemd unit or macOS subsystem/process
    QString message;

    static QString severityString(int severity);
};

class LogProvider : public QObject
{
    Q_OBJECT

public:
    explicit LogProvider(QObject *parent = nullptr);
    virtual ~LogProvider() = default;

    virtual void fetchLogs(int maxEntries = 500) = 0;
    virtual void cancel();
    bool isBusy() const { return mBusy; }

    static LogProvider *createForPlatform(QObject *parent = nullptr);

signals:
    void logsReady(const QList<LogEntry> &entries);
    void errorOccurred(const QString &message);

protected:
    QProcess *mProcess;
    bool mBusy;
};

class LogProviderLinux : public LogProvider
{
    Q_OBJECT
public:
    explicit LogProviderLinux(QObject *parent = nullptr);
    void fetchLogs(int maxEntries = 500) override;

private slots:
    void onProcessFinished(int exitCode, QProcess::ExitStatus status);
};

class LogProviderMacOS : public LogProvider
{
    Q_OBJECT
public:
    explicit LogProviderMacOS(QObject *parent = nullptr);
    void fetchLogs(int maxEntries = 500) override;

private slots:
    void onProcessFinished(int exitCode, QProcess::ExitStatus status);

private:
    int mMaxEntries = 500;
};

#endif // LOG_PROVIDER_H
