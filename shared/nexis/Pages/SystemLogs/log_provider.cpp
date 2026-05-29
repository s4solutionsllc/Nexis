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
    if (mProcess && mProcess->state() != QProcess::NotRunning) {
        mProcess->kill();
        mProcess->waitForFinished(3000);
        mProcess->deleteLater();
        mProcess = nullptr;
        mBusy = false;
    }
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
// LogProviderMacOS
// ---------------------------------------------------------------------------

LogProviderMacOS::LogProviderMacOS(QObject *parent)
    : LogProvider(parent)
{
}

void LogProviderMacOS::fetchLogs(int maxEntries, int /*maxSeverity*/)
{
    if (mBusy)
        return;

    mMaxEntries = maxEntries;

    mBusy = true;
    mProcess = new QProcess(this);

    connect(mProcess, &QProcess::finished,
            this, &LogProviderMacOS::onProcessFinished);

    mProcess->start(QStringLiteral("log"),
                    {QStringLiteral("show"),
                     QStringLiteral("--style"),
                     QStringLiteral("ndjson"),
                     QStringLiteral("--last"),
                     QStringLiteral("1h"),
                     QStringLiteral("--predicate"),
                     QStringLiteral("eventType == logEvent")});

    if (!mProcess->waitForStarted(3000)) {
        emit errorOccurred(tr("Failed to start macOS log: %1")
                               .arg(mProcess->errorString()));
        mProcess->deleteLater();
        mProcess = nullptr;
        mBusy = false;
    }
}

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

void LogProviderMacOS::onProcessFinished(int exitCode, QProcess::ExitStatus status)
{
    QByteArray output = mProcess->readAllStandardOutput();

    if (exitCode != 0 || status == QProcess::CrashExit) {
        QString errMsg = QString::fromUtf8(mProcess->readAllStandardError()).trimmed();
        emit errorOccurred(tr("macOS log failed (exit %1): %2").arg(exitCode).arg(errMsg));
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

        // Timestamp
        const QString tsStr = obj.value(QStringLiteral("timestamp")).toString();
        entry.timestamp = parseMacOsTimestamp(tsStr);

        // Severity from messageType
        const QString msgType = obj.value(QStringLiteral("messageType")).toString();
        entry.severity = macOsMessageTypeToSeverity(msgType);

        // Unit: prefer subsystem, fall back to process
        entry.unit = obj.value(QStringLiteral("subsystem")).toString();
        if (entry.unit.isEmpty())
            entry.unit = obj.value(QStringLiteral("process")).toString();

        // Message
        entry.message = obj.value(QStringLiteral("eventMessage")).toString();

        entries.append(entry);
    }

    // Sort descending by timestamp (newest first)
    std::sort(entries.begin(), entries.end(),
              [](const LogEntry &a, const LogEntry &b) {
                  return a.timestamp > b.timestamp;
              });

    // macOS log show has no --lines flag, so trim to the requested limit
    if (entries.size() > mMaxEntries)
        entries = entries.mid(0, mMaxEntries);

    emit logsReady(entries);

    mProcess->deleteLater();
    mProcess = nullptr;
    mBusy = false;
}
