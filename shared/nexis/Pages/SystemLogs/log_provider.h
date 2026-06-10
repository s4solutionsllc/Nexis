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

    virtual void fetchLogs(int maxEntries = 500, int maxSeverity = 7) = 0;
    virtual void cancel();   // base impl kills `mProcess`; subclasses may extend.
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
    void fetchLogs(int maxEntries = 500, int maxSeverity = 7) override;

private slots:
    void onProcessFinished(int exitCode, QProcess::ExitStatus status);
};

// WI-22 (SSO-3384): Incremental ndjson parser for `log show --style ndjson`.
//
// The previous implementation buffered the entire 1-hour log dump (hundreds
// of MB to >1 GB) before parsing, copied it twice, and parsed on the main
// thread. This class lets the provider feed stdout chunks as they arrive,
// parse complete newline-delimited records on the fly, and stop early once
// `maxEntries` records have been accepted. The internal buffer only holds
// the trailing partial line between chunks, so memory is bounded by
// `maxEntries * sizeof(LogEntry)` plus one line.
//
// The class is intentionally free of Qt signal/process plumbing so it can
// be exercised directly from tests/managers/test_log_provider.cpp.
class MacOsLogStreamParser
{
public:
    explicit MacOsLogStreamParser(int maxEntries);

    // Feed a chunk of stdout bytes. Returns true once `maxEntries` records
    // have been accepted (the caller should kill the QProcess at that point).
    bool feed(const QByteArray &chunk);

    // Flush any trailing buffered bytes that look like a complete ndjson
    // record (no trailing newline). Returns true if the cap was reached.
    bool finish();

    // Move out the accumulated entries (sorted descending by timestamp).
    QList<LogEntry> takeEntries();

    int retainedCount() const { return mEntries.size(); }
    int linesParsed() const { return mLinesParsed; }
    int linesDropped() const { return mLinesDropped; }
    int bufferedTailBytes() const { return mPartial.size(); }
    bool capReached() const { return mEntries.size() >= mMaxEntries; }

private:
    void processLine(const QByteArray &line);

    int mMaxEntries;
    QByteArray mPartial;       // partial trailing line between chunks
    QList<LogEntry> mEntries;
    int mLinesParsed = 0;
    int mLinesDropped = 0;
};

class LogProviderMacOS : public LogProvider
{
    Q_OBJECT
public:
    explicit LogProviderMacOS(QObject *parent = nullptr);
    ~LogProviderMacOS() override;
    void fetchLogs(int maxEntries = 500, int maxSeverity = 7) override;
    void cancel() override;

private slots:
    void onReadyRead();
    void onProcessFinished(int exitCode, QProcess::ExitStatus status);

private:
    int mMaxEntries = 500;
    bool mCapReached = false;       // we killed the process intentionally
    MacOsLogStreamParser *mParser = nullptr;
};

#endif // LOG_PROVIDER_H
