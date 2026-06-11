#include "log_provider.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <algorithm>

// ---------------------------------------------------------------------------
// LogEntry
// ---------------------------------------------------------------------------

QString LogEntry::severityString(int severity)
{
    switch (severity) {
    case 0: return QStringLiteral("EMERG");
    case 1: return QStringLiteral("ALERT");
    case 2: return QStringLiteral("CRIT");
    case 3: return QStringLiteral("ERR");
    case 4: return QStringLiteral("WARN");
    case 5: return QStringLiteral("NOTICE");
    case 6: return QStringLiteral("INFO");
    case 7: return QStringLiteral("DEBUG");
    default: return QStringLiteral("UNKNOWN");
    }
}

// ---------------------------------------------------------------------------
// LogProvider (base)
// ---------------------------------------------------------------------------

LogProvider::LogProvider(QObject *parent)
    : QObject(parent)
    , mProcess(nullptr)
    , mBusy(false)
{
}

void LogProvider::cancel()
{
    if (!mProcess)
        return;

    // Take ownership locally and null the member first, so that even if a
    // queued signal still drives onProcessFinished() later, it cannot
    // dereference a stale pointer or double-delete the QProcess.
    QProcess *p = mProcess;
    mProcess = nullptr;
    mBusy = false;

    // Disconnect before waitForFinished(): that call processes events on the
    // current thread, which would otherwise deliver finished() synchronously
    // and run onProcessFinished() — whose error path nulls mProcess and calls
    // deleteLater() while we are still mid-cancel, leading to a null deref
    // on the next mProcess access here (H2 in the 2026-06-10 audit).
    p->disconnect(this);

    if (p->state() != QProcess::NotRunning) {
        p->kill();
        p->waitForFinished(3000);
    }

    p->deleteLater();
}

LogProvider *LogProvider::createForPlatform(QObject *parent)
{
#ifdef Q_OS_MACOS
    return new LogProviderMacOS(parent);
#else
    return new LogProviderLinux(parent);
#endif
}

// ---------------------------------------------------------------------------
// LogProviderLinux
// ---------------------------------------------------------------------------

LogProviderLinux::LogProviderLinux(QObject *parent)
    : LogProvider(parent)
{
}

void LogProviderLinux::fetchLogs(int maxEntries, int maxSeverity)
{
    if (mBusy)
        return;

    mBusy = true;
    mProcess = new QProcess(this);

    connect(mProcess, &QProcess::finished,
            this, &LogProviderLinux::onProcessFinished);

    QStringList args = {
        QStringLiteral("--output=json"),
        QStringLiteral("--no-pager"),
        QStringLiteral("--lines=%1").arg(maxEntries),
        QStringLiteral("--reverse"),
    };
    if (maxSeverity < 7)
        args << QStringLiteral("--priority=0..%1").arg(maxSeverity);

    mProcess->start(QStringLiteral("journalctl"), args);

    if (!mProcess->waitForStarted(3000)) {
        emit errorOccurred(tr("Failed to start journalctl: %1")
                               .arg(mProcess->errorString()));
        mProcess->deleteLater();
        mProcess = nullptr;
        mBusy = false;
    }
}

void LogProviderLinux::onProcessFinished(int exitCode, QProcess::ExitStatus status)
{
    QByteArray output = mProcess->readAllStandardOutput();

    if (exitCode != 0 || status == QProcess::CrashExit) {
        QString errMsg = QString::fromUtf8(mProcess->readAllStandardError()).trimmed();
        emit errorOccurred(tr("journalctl failed (exit %1): %2").arg(exitCode).arg(errMsg));
        mProcess->deleteLater();
        mProcess = nullptr;
        mBusy = false;
        return;
    }

    QList<LogEntry> entries;
    const QList<QByteArray> lines = output.split('\n');

    for (const QByteArray &line : lines) {
        if (line.trimmed().isEmpty())
            continue;

        QJsonParseError parseErr;
        QJsonDocument doc = QJsonDocument::fromJson(line, &parseErr);
        if (parseErr.error != QJsonParseError::NoError || !doc.isObject())
            continue;

        QJsonObject obj = doc.object();

        LogEntry entry;

        // Timestamp: __REALTIME_TIMESTAMP is microseconds since epoch (as string)
        const QString usecStr = obj.value(QStringLiteral("__REALTIME_TIMESTAMP")).toString();
        bool ok = false;
        qint64 usec = usecStr.toLongLong(&ok);
        if (ok)
            entry.timestamp = QDateTime::fromMSecsSinceEpoch(usec / 1000);

        // Severity: PRIORITY is a string digit
        const QString prioStr = obj.value(QStringLiteral("PRIORITY")).toString();
        entry.severity = prioStr.toInt(&ok);
        if (!ok)
            entry.severity = 6; // default to INFO

        // Unit: prefer _SYSTEMD_UNIT, fall back to SYSLOG_IDENTIFIER
        entry.unit = obj.value(QStringLiteral("_SYSTEMD_UNIT")).toString();
        if (entry.unit.isEmpty())
            entry.unit = obj.value(QStringLiteral("SYSLOG_IDENTIFIER")).toString();

        // Message
        entry.message = obj.value(QStringLiteral("MESSAGE")).toString();

        entries.append(entry);
    }

    emit logsReady(entries);

    mProcess->deleteLater();
    mProcess = nullptr;
    mBusy = false;
}

// ---------------------------------------------------------------------------
// macOS helpers
// ---------------------------------------------------------------------------

static int macOsMessageTypeToSeverity(const QString &type)
{
    if (type == QLatin1String("Fault"))   return 2;
    if (type == QLatin1String("Error"))   return 3;
    if (type == QLatin1String("Default")) return 5;
    if (type == QLatin1String("Info"))    return 6;
    if (type == QLatin1String("Debug"))   return 7;
    return 6; // fallback to INFO
}

static QDateTime parseMacOsTimestamp(const QString &str)
{
    // macOS log ndjson timestamps vary in format. Try several strategies:

    // Strategy 1: ISO 8601 with milliseconds
    QDateTime dt = QDateTime::fromString(str, Qt::ISODateWithMs);
    if (dt.isValid())
        return dt;

    // Strategy 2: "yyyy-MM-dd HH:mm:ss.ffffffZ" style with timezone offset
    // The 't' format spec matches timezone offset like +0000 or -0700
    dt = QDateTime::fromString(str, QStringLiteral("yyyy-MM-dd HH:mm:ss.zzzzzzt"));
    if (dt.isValid())
        return dt;

    // Strategy 3: simpler "yyyy-MM-dd HH:mm:ss.zzz" (just milliseconds)
    dt = QDateTime::fromString(str, QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz"));
    if (dt.isValid())
        return dt;

    // Strategy 4: no fractional seconds
    dt = QDateTime::fromString(str, QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    if (dt.isValid())
        return dt;

    return QDateTime(); // invalid
}

// ---------------------------------------------------------------------------
// MacOsLogStreamParser
// ---------------------------------------------------------------------------

MacOsLogStreamParser::MacOsLogStreamParser(int maxEntries)
    : mMaxEntries(maxEntries > 0 ? maxEntries : 1)
{
    mEntries.reserve(mMaxEntries);
}

bool MacOsLogStreamParser::feed(const QByteArray &chunk)
{
    if (capReached())
        return true;

    mPartial.append(chunk);

    int start = 0;
    while (start < mPartial.size()) {
        const int nl = mPartial.indexOf('\n', start);
        if (nl < 0)
            break;

        // QByteArray::mid copies; cheap for a single ndjson line.
        processLine(mPartial.mid(start, nl - start));
        start = nl + 1;

        if (capReached())
            break;
    }

    // Drop everything we've already consumed so the buffer stays bounded
    // to at most one in-flight (partial) line.
    if (start > 0)
        mPartial.remove(0, start);

    // Once the cap is reached we no longer need to retain trailing bytes —
    // the caller is expected to kill the QProcess immediately after.
    if (capReached())
        mPartial.clear();

    return capReached();
}

bool MacOsLogStreamParser::finish()
{
    if (capReached()) {
        mPartial.clear();
        return true;
    }
    if (!mPartial.isEmpty()) {
        processLine(mPartial);
        mPartial.clear();
    }
    return capReached();
}

void MacOsLogStreamParser::processLine(const QByteArray &line)
{
    const QByteArray trimmed = line.trimmed();
    if (trimmed.isEmpty())
        return;

    QJsonParseError parseErr;
    const QJsonDocument doc = QJsonDocument::fromJson(trimmed, &parseErr);
    if (parseErr.error != QJsonParseError::NoError || !doc.isObject()) {
        ++mLinesDropped;
        return;
    }

    const QJsonObject obj = doc.object();

    LogEntry entry;
    entry.timestamp = parseMacOsTimestamp(
        obj.value(QStringLiteral("timestamp")).toString());
    entry.severity = macOsMessageTypeToSeverity(
        obj.value(QStringLiteral("messageType")).toString());
    entry.unit = obj.value(QStringLiteral("subsystem")).toString();
    if (entry.unit.isEmpty())
        entry.unit = obj.value(QStringLiteral("process")).toString();
    entry.message = obj.value(QStringLiteral("eventMessage")).toString();

    mEntries.append(entry);
    ++mLinesParsed;
}

QList<LogEntry> MacOsLogStreamParser::takeEntries()
{
    // Sort descending by timestamp (newest first) — matches the contract the
    // page used to get from the post-finished sort+trim.
    std::sort(mEntries.begin(), mEntries.end(),
              [](const LogEntry &a, const LogEntry &b) {
                  return a.timestamp > b.timestamp;
              });
    return std::move(mEntries);
}

// ---------------------------------------------------------------------------
// LogProviderMacOS
// ---------------------------------------------------------------------------

LogProviderMacOS::LogProviderMacOS(QObject *parent)
    : LogProvider(parent)
{
}

LogProviderMacOS::~LogProviderMacOS()
{
    delete mParser;
    mParser = nullptr;
}

void LogProviderMacOS::cancel()
{
    LogProvider::cancel();
    delete mParser;
    mParser = nullptr;
    mCapReached = false;
}

void LogProviderMacOS::fetchLogs(int maxEntries, int maxSeverity)
{
    if (mBusy)
        return;

    mMaxEntries = maxEntries;
    mCapReached = false;
    delete mParser;
    mParser = new MacOsLogStreamParser(maxEntries);

    mBusy = true;
    mProcess = new QProcess(this);

    connect(mProcess, &QProcess::readyReadStandardOutput,
            this, &LogProviderMacOS::onReadyRead);
    connect(mProcess, &QProcess::finished,
            this, &LogProviderMacOS::onProcessFinished);

    // Shrink the default window from 1h to 5m: an idle Mac produces well
    // under 500 entries in 5 minutes, and a busy Mac will cap quickly via
    // the streaming early-stop path. The previous 1h window was hundreds
    // of MB of ndjson — the whole reason for WI-22.
    QStringList args = {
        QStringLiteral("show"),
        QStringLiteral("--style"), QStringLiteral("ndjson"),
        QStringLiteral("--last"), QStringLiteral("5m"),
        QStringLiteral("--predicate"), QStringLiteral("eventType == logEvent"),
    };

    // `log show` excludes Info and Debug records by default. Only ask for
    // them when the active severity filter would actually surface them —
    // this is the level-filtering the audit asked for.
    if (maxSeverity >= 6)
        args << QStringLiteral("--info");
    if (maxSeverity >= 7)
        args << QStringLiteral("--debug");

    mProcess->start(QStringLiteral("log"), args);

    if (!mProcess->waitForStarted(3000)) {
        emit errorOccurred(tr("Failed to start macOS log: %1")
                               .arg(mProcess->errorString()));
        mProcess->deleteLater();
        mProcess = nullptr;
        delete mParser;
        mParser = nullptr;
        mBusy = false;
    }
}

void LogProviderMacOS::onReadyRead()
{
    if (!mProcess || !mParser || mCapReached)
        return;

    // Pull whatever stdout is currently available and feed it to the
    // parser. Each callback only sees a small chunk, so per-tick work
    // on the main thread is bounded — no more giant end-of-process parse.
    if (mParser->feed(mProcess->readAllStandardOutput())) {
        mCapReached = true;
        // We've got our `mMaxEntries`; stop log show before it produces
        // more output we'd just throw away. onProcessFinished will run.
        mProcess->kill();
    }
}

void LogProviderMacOS::onProcessFinished(int exitCode, QProcess::ExitStatus status)
{
    // Drain anything that arrived between the last readyRead and finished().
    if (mProcess && mParser)
        mParser->feed(mProcess->readAllStandardOutput());
    if (mParser)
        mParser->finish();

    // A non-zero exit / crash is only a real error if we did NOT kill the
    // process ourselves after hitting the entry cap.
    if (!mCapReached && (exitCode != 0 || status == QProcess::CrashExit)) {
        QString errMsg = QString::fromUtf8(mProcess->readAllStandardError()).trimmed();
        emit errorOccurred(tr("macOS log failed (exit %1): %2").arg(exitCode).arg(errMsg));
        delete mParser;
        mParser = nullptr;
        mProcess->deleteLater();
        mProcess = nullptr;
        mBusy = false;
        return;
    }

    QList<LogEntry> entries = mParser ? mParser->takeEntries() : QList<LogEntry>{};
    delete mParser;
    mParser = nullptr;

    emit logsReady(entries);

    mProcess->deleteLater();
    mProcess = nullptr;
    mBusy = false;
}
