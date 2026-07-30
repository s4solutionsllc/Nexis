#include "uninstall_audit_log.h"

#include <QCoreApplication>
#include <QDate>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMutex>
#include <QMutexLocker>
#include <QStandardPaths>
#include <QTextStream>

namespace UninstallAuditLog {

namespace {

static QMutex s_mutex;
static bool   s_pruned = false;

static QString logDir()
{
    const QString appData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return appData + QLatin1String("/UninstallAuditLog");
}

static QString todayLogPath()
{
    return logDir() + QLatin1Char('/') + QDate::currentDate().toString(Qt::ISODate) + QLatin1String(".jsonl");
}

} // namespace

void pruneOldLogs()
{
    QDir dir(logDir());
    if (!dir.exists())
        return;
    const QDate cutoff = QDate::currentDate().addDays(-90);
    const QFileInfoList files = dir.entryInfoList(QStringList() << QStringLiteral("*.jsonl"),
                                                  QDir::Files | QDir::NoDotAndDotDot);
    for (const QFileInfo &fi : files) {
        const QDate fileDate = QDate::fromString(fi.completeBaseName(), Qt::ISODate);
        if (fileDate.isValid() && fileDate < cutoff)
            dir.remove(fi.fileName());
    }
}

void append(const Entry &entry)
{
    QMutexLocker lock(&s_mutex);

    const QString dir = logDir();
    QDir().mkpath(dir);

    // Restrict permissions: owner read/write only so the log cannot be world-read.
    const QString path = todayLogPath();
    QFile file(path);
    if (!file.open(QIODevice::Append | QIODevice::Text)) {
        qCritical() << "UninstallAuditLog: could not open log file:" << path;
        return;
    }

    // On first open, set permissions to 0600.
    if (file.size() == 0) {
        file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    }

    QJsonObject obj;
    obj[QLatin1String("ts")]            = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    obj[QLatin1String("batchId")]       = entry.batchId.toString(QUuid::WithoutBraces);
    obj[QLatin1String("originalPath")]  = entry.originalPath;
    obj[QLatin1String("canonicalPath")] = entry.canonicalPath;
    obj[QLatin1String("action")]        = (entry.action == Action::MovedToTrash)
                                          ? QLatin1String("moved_to_trash")
                                          : QLatin1String("permanently_deleted");
    if (!entry.trashedPath.isEmpty())
        obj[QLatin1String("trashedPath")] = entry.trashedPath;
    obj[QLatin1String("matchedRule")]   = entry.matchedRule;
    obj[QLatin1String("sizeBytes")]     = static_cast<qint64>(entry.sizeBytes);
    obj[QLatin1String("nexisVersion")]  = entry.nexisVersion.isEmpty()
                                          ? QCoreApplication::applicationVersion()
                                          : entry.nexisVersion;

    QTextStream ts(&file);
    ts << QJsonDocument(obj).toJson(QJsonDocument::Compact) << '\n';
    file.close();

    // Prune once per process lifetime to avoid slowing every write.
    if (!s_pruned) {
        s_pruned = true;
        pruneOldLogs();
    }
}

} // namespace UninstallAuditLog
