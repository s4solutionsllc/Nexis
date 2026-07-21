#include "leftover_deny_list.h"

#include <QDir>
#include <QFile>
#include <QMutex>
#include <QStandardPaths>
#include <QTextStream>

namespace LeftoverDenyList {

QString auditLogPath()
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)
                        + QLatin1String("/audit");
    QDir().mkpath(dir);
    return dir + QLatin1String("/leftover_deletions.log");
}

void logDeletion(const AuditEntry &entry)
{
    static QMutex mutex;
    QMutexLocker lock(&mutex);

    QFile f(auditLogPath());
    if (!f.open(QIODevice::Append | QIODevice::Text))
        return;

    const QDateTime timestamp = entry.timestamp.isValid()
        ? entry.timestamp
        : QDateTime::currentDateTimeUtc();

    QTextStream out(&f);
    out << "time=" << timestamp.toString(Qt::ISODateWithMs)
        << " batch=" << entry.batchId
        << " original=" << entry.originalPath
        << " canonical=" << entry.canonicalPath
        << " action=" << entry.action
        << " trash_dest=" << entry.trashDest
        << " rule=" << entry.matchRule
        << " size=" << entry.sizeBytes
        << " nexis=" << entry.nexisVersion
        << "\n";
}

} // namespace LeftoverDenyList
