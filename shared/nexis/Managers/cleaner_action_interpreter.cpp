#include "cleaner_action_interpreter.h"

#include <Utils/file_util.h>
#include <Utils/sandboxed_path_resolver.h>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QObject>
#include <QRegularExpression>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QUuid>

#include <algorithm>

using namespace CleanerML;

namespace {

constexpr const char *ID_PREFIX_DELETE   = "delete::";
constexpr const char *ID_PREFIX_TRUNCATE = "truncate::";
constexpr const char *ID_PREFIX_VACUUM   = "vacuum::";

// SQLite's own way of estimating how much VACUUM would reclaim, without
// running it: free pages still allocated in the file times the page size.
// Falls back to -1 (unknown) if the DB can't be opened read-only.
qint64 estimateVacuumReclaimableBytes(const QString &dbPath)
{
    const QString connName = QStringLiteral("cleaner-vacuum-estimate-") + QUuid::createUuid().toString();
    qint64 estimate = -1;
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connName);
        db.setDatabaseName(dbPath);
        db.setConnectOptions(QStringLiteral("QSQLITE_OPEN_READONLY"));
        if (db.open()) {
            QSqlQuery freelist(db);
            QSqlQuery pageSize(db);
            if (freelist.exec(QStringLiteral("PRAGMA freelist_count")) && freelist.next()
                && pageSize.exec(QStringLiteral("PRAGMA page_size")) && pageSize.next()) {
                estimate = freelist.value(0).toLongLong() * pageSize.value(0).toLongLong();
            }
            db.close();
        }
    }
    QSqlDatabase::removeDatabase(connName);
    return estimate;
}

} // namespace

CleanerActionInterpreter::SandboxRoots CleanerActionInterpreter::defaultSandboxRoots()
{
    SandboxRoots roots;
    roots.home = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    roots.cache = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    return roots;
}

CleanerActionInterpreter::CleanerActionInterpreter(Cleaner cleaner,
                                                     QSet<QString> selectedOptionIds,
                                                     SandboxRoots sandboxRoots)
    : mCleaner(std::move(cleaner))
    , mSelectedOptionIds(std::move(selectedOptionIds))
    , mSandboxRoots(std::move(sandboxRoots))
{
}

QString CleanerActionInterpreter::expandVariables(const QString &rawPath) const
{
    static const QRegularExpression tokenRe(QStringLiteral("\\$\\$([A-Za-z0-9_-]+)\\$\\$"));

    QString result = rawPath;
    qsizetype searchFrom = 0;
    for (;;) {
        const QRegularExpressionMatch m = tokenRe.match(result, searchFrom);
        if (!m.hasMatch())
            break;

        const QString token = m.captured(1).toLower();
        QString replacement;
        if (token == QLatin1String("home"))
            replacement = mSandboxRoots.home;
        else if (token == QLatin1String("cache"))
            replacement = mSandboxRoots.cache;
        else
            return QString(); // app-specific token this interpreter can't resolve

        if (replacement.isEmpty())
            return QString();

        result.replace(m.capturedStart(), m.capturedLength(), replacement);
        searchFrom = m.capturedStart() + replacement.length();
    }
    return result;
}

QString CleanerActionInterpreter::confiningRoot(const QString &candidatePath) const
{
    for (const QString &root : {mSandboxRoots.home, mSandboxRoots.cache}) {
        if (!root.isEmpty() && SandboxedPathResolver::isPathConfinedTo(candidatePath, root))
            return root;
    }
    return QString();
}

void CleanerActionInterpreter::scan(
    QAtomicInt *cancelled,
    const std::function<void(const TrustSafetyActionItem &)> &itemFound)
{
    for (const Option &option : mCleaner.options) {
        if (!mSelectedOptionIds.contains(option.id))
            continue;

        for (const Action &action : option.actions) {
            if (cancelled && cancelled->loadRelaxed())
                return;

            switch (action.type) {
            case ActionType::Delete:
                scanLiteralPath(action, /*isTruncate=*/false, itemFound);
                break;
            case ActionType::Truncate:
                scanLiteralPath(action, /*isTruncate=*/true, itemFound);
                break;
            case ActionType::Glob:
            case ActionType::Walk:
            case ActionType::Regex:
                scanPattern(action, itemFound);
                break;
            case ActionType::SqliteVacuum:
                scanVacuum(action, itemFound);
                break;
            case ActionType::Winreg:
                // Never reaches here — CleanerMLParser filters winreg out of
                // Option::actions before this model is built.
                break;
            }
        }
    }
}

void CleanerActionInterpreter::scanLiteralPath(
    const Action &action, bool isTruncate,
    const std::function<void(const TrustSafetyActionItem &)> &itemFound)
{
    const QString expanded = expandVariables(action.path);
    if (expanded.isEmpty() || confiningRoot(expanded).isEmpty())
        return;

    QFileInfo info(expanded);
    if (!info.exists())
        return;

    TrustSafetyActionItem item;
    item.id                 = QLatin1String(isTruncate ? ID_PREFIX_TRUNCATE : ID_PREFIX_DELETE) + expanded;
    item.label               = info.fileName();
    item.description         = isTruncate
        ? QObject::tr("File will be truncated to zero bytes.")
        : QObject::tr("File or directory will be permanently deleted.");
    item.command             = (isTruncate ? QStringLiteral("truncate --size 0 ") : QStringLiteral("rm -rf ")) + expanded;
    item.categoryId          = mCleaner.id;
    item.categoryLabel       = mCleaner.label;
    item.riskTier            = TrustSafetyActionItem::RiskTier::Standard;
    item.estimatedSizeBytes  = static_cast<qint64>(FileUtil::getFileSize(expanded));
    itemFound(item);
}

void CleanerActionInterpreter::scanPattern(
    const Action &action,
    const std::function<void(const TrustSafetyActionItem &)> &itemFound)
{
    QString dirPath;
    QString pattern;

    if (action.type == ActionType::Glob) {
        const QString expanded = expandVariables(action.path);
        if (expanded.isEmpty())
            return;
        QFileInfo info(expanded);
        dirPath = info.path();
        pattern = info.fileName();
    } else {
        // Walk and Regex: `path` names the directory to search; Regex's
        // filter comes from the separate `regex` field.
        dirPath = expandVariables(action.path);
        if (dirPath.isEmpty())
            return;
        pattern = action.regex;
    }

    if (confiningRoot(dirPath).isEmpty())
        return;

    const auto kind = action.type == ActionType::Glob   ? SandboxedPathResolver::MatchKind::Glob
                     : action.type == ActionType::Regex ? SandboxedPathResolver::MatchKind::Regex
                                                          : SandboxedPathResolver::MatchKind::Walk;

    const auto matches = SandboxedPathResolver::resolve(dirPath, QString(), pattern, kind);
    for (const auto &match : matches) {
        TrustSafetyActionItem item;
        item.id                 = QLatin1String(ID_PREFIX_DELETE) + match.absolutePath;
        item.label               = QFileInfo(match.absolutePath).fileName();
        item.description         = QObject::tr("File or directory will be permanently deleted.");
        item.command             = QStringLiteral("rm -f ") + match.absolutePath;
        item.categoryId          = mCleaner.id;
        item.categoryLabel       = mCleaner.label;
        item.riskTier            = TrustSafetyActionItem::RiskTier::Standard;
        item.estimatedSizeBytes  = match.sizeBytes;
        itemFound(item);
    }
}

void CleanerActionInterpreter::scanVacuum(
    const Action &action,
    const std::function<void(const TrustSafetyActionItem &)> &itemFound)
{
    QStringList dbPaths;

    if (action.search == QStringLiteral("glob")) {
        const QString expanded = expandVariables(action.path);
        if (expanded.isEmpty())
            return;
        QFileInfo info(expanded);
        const QString dirPath = info.path();
        if (confiningRoot(dirPath).isEmpty())
            return;
        const auto matches = SandboxedPathResolver::resolve(
            dirPath, QString(), info.fileName(), SandboxedPathResolver::MatchKind::Glob);
        for (const auto &match : matches)
            dbPaths << match.absolutePath;
    } else {
        const QString expanded = expandVariables(action.path);
        if (expanded.isEmpty() || confiningRoot(expanded).isEmpty())
            return;
        if (QFileInfo::exists(expanded))
            dbPaths << expanded;
    }

    for (const QString &dbPath : dbPaths) {
        TrustSafetyActionItem item;
        item.id                 = QLatin1String(ID_PREFIX_VACUUM) + dbPath;
        item.label               = QFileInfo(dbPath).fileName();
        item.description         = QObject::tr("Database will be compacted (VACUUM); no rows are removed by this action alone.");
        item.command             = QStringLiteral("sqlite3 ") + dbPath + QStringLiteral(" 'VACUUM;'");
        item.categoryId          = mCleaner.id;
        item.categoryLabel       = mCleaner.label;
        item.riskTier            = TrustSafetyActionItem::RiskTier::Standard;
        const qint64 estimate = estimateVacuumReclaimableBytes(dbPath);
        item.estimatedSizeBytes = estimate >= 0 ? estimate : static_cast<qint64>(FileUtil::getFileSize(dbPath));
        itemFound(item);
    }
}

TrustSafetyActionResult CleanerActionInterpreter::performItem(const TrustSafetyActionItem &item, bool dryRun)
{
    TrustSafetyActionResult result;
    result.itemId = item.id;

    if (item.id.startsWith(QLatin1String(ID_PREFIX_DELETE))) {
        const QString path = item.id.mid(qstrlen(ID_PREFIX_DELETE));

        // Defense in depth: re-verify confinement right before touching disk,
        // even though scan() already gated discovery on it.
        if (confiningRoot(path).isEmpty()) {
            result.error = QObject::tr("Path is outside the sandboxed roots: %1").arg(path);
            return result;
        }

        const qint64 sizeBefore = static_cast<qint64>(FileUtil::getFileSize(path));
        if (dryRun) {
            result.succeeded = QFileInfo::exists(path);
            result.bytesFreed = sizeBefore;
        } else {
            QFileInfo info(path);
            const bool removed = info.isDir() ? QDir(path).removeRecursively() : QFile::remove(path);
            result.succeeded = removed || !QFileInfo::exists(path);
            result.bytesFreed = sizeBefore;
            if (!result.succeeded)
                result.error = QObject::tr("Failed to remove: %1").arg(path);
        }
        return result;
    }

    if (item.id.startsWith(QLatin1String(ID_PREFIX_TRUNCATE))) {
        const QString path = item.id.mid(qstrlen(ID_PREFIX_TRUNCATE));

        if (confiningRoot(path).isEmpty()) {
            result.error = QObject::tr("Path is outside the sandboxed roots: %1").arg(path);
            return result;
        }

        const qint64 sizeBefore = static_cast<qint64>(FileUtil::getFileSize(path));
        if (dryRun) {
            result.succeeded = QFileInfo::exists(path);
            result.bytesFreed = sizeBefore;
        } else {
            QFile file(path);
            result.succeeded = file.open(QIODevice::WriteOnly | QIODevice::Truncate);
            file.close();
            result.bytesFreed = result.succeeded ? sizeBefore : 0;
            if (!result.succeeded)
                result.error = QObject::tr("Failed to truncate: %1").arg(path);
        }
        return result;
    }

    if (item.id.startsWith(QLatin1String(ID_PREFIX_VACUUM))) {
        const QString dbPath = item.id.mid(qstrlen(ID_PREFIX_VACUUM));

        if (confiningRoot(dbPath).isEmpty()) {
            result.error = QObject::tr("Path is outside the sandboxed roots: %1").arg(dbPath);
            return result;
        }

        if (dryRun) {
            const qint64 estimate = estimateVacuumReclaimableBytes(dbPath);
            result.succeeded = QFileInfo::exists(dbPath);
            result.bytesFreed = estimate >= 0 ? estimate : 0;
            return result;
        }

        const qint64 sizeBefore = static_cast<qint64>(FileUtil::getFileSize(dbPath));
        const QString connName = QStringLiteral("cleaner-vacuum-") + QUuid::createUuid().toString();
        {
            QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connName);
            db.setDatabaseName(dbPath);
            if (!db.open()) {
                result.error = db.lastError().text();
            } else {
                // VACUUM must run outside any explicit transaction.
                QSqlQuery vacuum(db);
                if (!vacuum.exec(QStringLiteral("VACUUM"))) {
                    result.error = vacuum.lastError().text();
                } else {
                    result.succeeded = true;
                }
                db.close();
            }
        }
        QSqlDatabase::removeDatabase(connName);

        if (result.succeeded) {
            const qint64 sizeAfter = static_cast<qint64>(FileUtil::getFileSize(dbPath));
            result.bytesFreed = std::max<qint64>(0, sizeBefore - sizeAfter);
        }
        return result;
    }

    result.error = QObject::tr("Unknown item id: %1").arg(item.id);
    return result;
}
