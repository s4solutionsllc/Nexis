#include "cleaner_action_interpreter.h"

#include <Tools/lifecycle_deny_list.h>
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

constexpr int kMaxVarDepth = 4;
constexpr int kMaxExpansions = 256;

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
    roots.cache = QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation);
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

void CleanerActionInterpreter::setExclusions(const QList<CleanerService::ExclusionEntry> &exclusions)
{
    mExclusions = exclusions;
}

QString CleanerActionInterpreter::expandEnvironment(const QString &path) const
{
    QString result = path;
    if (result == QLatin1String("~") || result.startsWith(QLatin1String("~/")))
        result.replace(0, 1, mSandboxRoots.home);

    // An XDG variable from the real environment is honoured only when it
    // points inside the sandbox home; otherwise the spec default applies.
    const auto xdg = [this](const char *name, const QString &fallback) {
        const QString fromEnv = qEnvironmentVariable(name);
        if (!fromEnv.isEmpty() && SandboxedPathResolver::isPathConfinedTo(fromEnv, mSandboxRoots.home))
            return fromEnv;
        return fallback;
    };
    const QList<QPair<QString, QString>> known = {
        {QStringLiteral("$XDG_CONFIG_HOME"), xdg("XDG_CONFIG_HOME", mSandboxRoots.home + QStringLiteral("/.config"))},
        {QStringLiteral("$XDG_CACHE_HOME"), mSandboxRoots.cache.isEmpty()
                                                 ? xdg("XDG_CACHE_HOME", mSandboxRoots.home + QStringLiteral("/.cache"))
                                                 : mSandboxRoots.cache},
        {QStringLiteral("$XDG_DATA_HOME"), xdg("XDG_DATA_HOME", mSandboxRoots.home + QStringLiteral("/.local/share"))},
        {QStringLiteral("$HOME"), mSandboxRoots.home},
    };
    for (const auto &[token, value] : known)
        result.replace(token, value);

    if (result.contains(QLatin1Char('$')) || result.contains(QLatin1Char('%')))
        return QString();
    return QDir::cleanPath(result);
}

QStringList CleanerActionInterpreter::expandVar(const QString &name, int depth) const
{
    QStringList results;
    for (const VarValue &value : mCleaner.vars.value(name)) {
        for (const QString &candidate : expandTokens(value.value, depth + 1)) {
            if (!value.glob) {
                results << candidate;
                continue;
            }
            // The last component is a pattern; expand to matching entries.
            const QFileInfo info(candidate);
            const QDir dir(info.path());
            if (confiningRoot(dir.path()).isEmpty() || !dir.exists())
                continue;
            const QStringList names = dir.entryList({info.fileName()},
                QDir::Dirs | QDir::Files | QDir::Hidden | QDir::NoDotAndDotDot, QDir::Name);
            for (const QString &entry : names)
                results << dir.absoluteFilePath(entry);
        }
    }
    return results;
}

QStringList CleanerActionInterpreter::expandTokens(const QString &rawPath, int depth) const
{
    // A <var> may refer to another one; anything deeper than this is a cycle.
    if (depth > kMaxVarDepth)
        return {};

    static const QRegularExpression tokenRe(QStringLiteral("\\$\\$([A-Za-z0-9_-]+)\\$\\$"));
    const QRegularExpressionMatch m = tokenRe.match(rawPath);
    if (!m.hasMatch()) {
        const QString expanded = expandEnvironment(rawPath);
        return expanded.isEmpty() ? QStringList() : QStringList{expanded};
    }

    const QString token = m.captured(1).toLower();
    QStringList replacements;
    if (token == QLatin1String("home"))
        replacements << mSandboxRoots.home;
    else if (token == QLatin1String("cache"))
        replacements << mSandboxRoots.cache;
    else
        replacements = expandVar(token, depth);

    QStringList results;
    for (const QString &replacement : std::as_const(replacements)) {
        if (replacement.isEmpty())
            continue;
        QString next = rawPath;
        next.replace(m.capturedStart(), m.capturedLength(), replacement);
        results << expandTokens(next, depth);
        if (results.size() > kMaxExpansions)
            return {};
    }
    return results;
}

QStringList CleanerActionInterpreter::expandPath(const QString &rawPath) const
{
    if (rawPath.isEmpty())
        return {};
    QStringList paths = expandTokens(rawPath, 0);
    paths.removeDuplicates();
    return paths;
}

bool CleanerActionInterpreter::isAllowedTarget(const QString &path) const
{
    const QString root = confiningRoot(path);
    if (root.isEmpty())
        return false;
    // The roots themselves are never a target, whatever a definition says.
    const QString clean = QDir::cleanPath(path);
    for (const QString &r : {mSandboxRoots.home, mSandboxRoots.cache}) {
        if (!r.isEmpty() && clean == QDir::cleanPath(r))
            return false;
    }
    return !CleanerService::isExcluded(path, mExclusions);
}

bool CleanerActionInterpreter::passesDenyList(const QString &path) const
{
    return LifecycleDenyList::isSafe(path, confiningRoot(path));
}

void CleanerActionInterpreter::emitItem(TrustSafetyActionItem item, const Option &option,
                                        const ItemSink &itemFound) const
{
    item.categoryId = mCleaner.id;
    item.categoryLabel = mCleaner.label;

    const QString warning = option.warning.isEmpty() ? mCleaner.warning : option.warning;
    item.riskTier = warning.isEmpty() ? TrustSafetyActionItem::RiskTier::Standard
                                      : TrustSafetyActionItem::RiskTier::Risky;
    if (!option.label.isEmpty())
        item.description = option.label + QStringLiteral(": ") + item.description;
    if (!warning.isEmpty())
        item.description += QLatin1Char(' ') + warning;
    itemFound(item);
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
                scanLiteralPath(option, action, /*isTruncate=*/false, itemFound);
                break;
            case ActionType::Truncate:
                scanLiteralPath(option, action, /*isTruncate=*/true, itemFound);
                break;
            case ActionType::Glob:
            case ActionType::Walk:
            case ActionType::Regex:
                scanPattern(option, action, itemFound);
                break;
            case ActionType::SqliteVacuum:
                scanVacuum(option, action, itemFound);
                break;
            case ActionType::Winreg:
            case ActionType::Unsupported:
                // Winreg never reaches here (the parser filters it out), and
                // an option holding an unsupported action is dropped whole
                // by CleanerML::forPlatform(); neither is ever executed.
                break;
            }
        }
    }
}

void CleanerActionInterpreter::scanLiteralPath(const Option &option, const Action &action,
                                               bool isTruncate, const ItemSink &itemFound)
{
    for (const QString &expanded : expandPath(action.path)) {
        if (!isAllowedTarget(expanded))
            continue;

        const QFileInfo info(expanded);
        if (!info.exists() || !passesDenyList(expanded))
            continue;

        TrustSafetyActionItem item;
        item.id = QLatin1String(isTruncate ? ID_PREFIX_TRUNCATE : ID_PREFIX_DELETE) + expanded;
        item.label = info.fileName();
        item.description = isTruncate
            ? QObject::tr("File will be truncated to zero bytes.")
            : QObject::tr("File or directory will be permanently deleted.");
        item.command = (isTruncate ? QStringLiteral("truncate --size 0 ") : QStringLiteral("rm -rf ")) + expanded;
        item.estimatedSizeBytes = static_cast<qint64>(FileUtil::getFileSize(expanded));
        emitItem(item, option, itemFound);
    }
}

void CleanerActionInterpreter::scanPattern(const Option &option, const Action &action,
                                           const ItemSink &itemFound)
{
    const auto kind = action.type == ActionType::Glob   ? SandboxedPathResolver::MatchKind::Glob
                     : action.type == ActionType::Regex ? SandboxedPathResolver::MatchKind::Regex
                                                          : SandboxedPathResolver::MatchKind::Walk;

    for (const QString &expanded : expandPath(action.path)) {
        QString dirPath = expanded;
        QString pattern = action.regex;
        if (action.type == ActionType::Glob) {
            const QFileInfo info(expanded);
            dirPath = info.path();
            pattern = info.fileName();
        }
        // Walk and Regex: the path names the directory to search; Regex's
        // filter comes from the separate `regex` field.

        if (confiningRoot(dirPath).isEmpty() || !QFileInfo(dirPath).isDir() || !passesDenyList(dirPath))
            continue;

        const auto matches = SandboxedPathResolver::resolve(dirPath, QString(), pattern, kind);
        for (const auto &match : matches) {
            if (!isAllowedTarget(match.absolutePath))
                continue;

            TrustSafetyActionItem item;
            item.id = QLatin1String(ID_PREFIX_DELETE) + match.absolutePath;
            item.label = QFileInfo(match.absolutePath).fileName();
            item.description = QObject::tr("File or directory will be permanently deleted.");
            item.command = QStringLiteral("rm -f ") + match.absolutePath;
            item.estimatedSizeBytes = match.sizeBytes;
            emitItem(item, option, itemFound);
        }
    }
}

void CleanerActionInterpreter::scanVacuum(const Option &option, const Action &action,
                                          const ItemSink &itemFound)
{
    QStringList dbPaths;
    for (const QString &expanded : expandPath(action.path)) {
        if (action.search == QStringLiteral("glob")) {
            const QFileInfo info(expanded);
            const QString dirPath = info.path();
            if (confiningRoot(dirPath).isEmpty())
                continue;
            const auto matches = SandboxedPathResolver::resolve(
                dirPath, QString(), info.fileName(), SandboxedPathResolver::MatchKind::Glob);
            for (const auto &match : matches)
                dbPaths << match.absolutePath;
        } else if (QFileInfo::exists(expanded)) {
            dbPaths << expanded;
        }
    }

    for (const QString &dbPath : std::as_const(dbPaths)) {
        if (!isAllowedTarget(dbPath) || !passesDenyList(dbPath))
            continue;

        TrustSafetyActionItem item;
        item.id = QLatin1String(ID_PREFIX_VACUUM) + dbPath;
        item.label = QFileInfo(dbPath).fileName();
        item.description = QObject::tr("Database will be compacted (VACUUM); no rows are removed by this action alone.");
        item.command = QStringLiteral("sqlite3 ") + dbPath + QStringLiteral(" 'VACUUM;'");
        const qint64 estimate = estimateVacuumReclaimableBytes(dbPath);
        item.estimatedSizeBytes = estimate >= 0 ? estimate : static_cast<qint64>(FileUtil::getFileSize(dbPath));
        emitItem(item, option, itemFound);
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
        if (!isAllowedTarget(path)) {
            result.error = QObject::tr("Path is outside the sandboxed roots or excluded: %1").arg(path);
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

        if (!isAllowedTarget(path)) {
            result.error = QObject::tr("Path is outside the sandboxed roots or excluded: %1").arg(path);
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

        if (!isAllowedTarget(dbPath)) {
            result.error = QObject::tr("Path is outside the sandboxed roots or excluded: %1").arg(dbPath);
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
