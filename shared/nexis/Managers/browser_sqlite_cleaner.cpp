#include "browser_sqlite_cleaner.h"

#include <Utils/file_util.h>

#include <QFileInfo>
#include <QObject>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStringList>
#include <QUuid>

namespace {

constexpr const char *ID_PREFIX_HISTORY = "history::";
constexpr const char *ID_PREFIX_COOKIES = "cookies::";
constexpr const char *TOKEN_FIREFOX = "firefox::";
constexpr const char *TOKEN_CHROMIUM = "chromium::";

// Max distinct domain names spelled out in preview description text before
// falling back to "+N more" — keeps the label readable for profiles with
// thousands of cookie domains.
constexpr int kMaxDomainsInDescription = 8;

QString familyToken(BrowserProfileLocator::Family family)
{
    return family == BrowserProfileLocator::Family::Firefox ? QLatin1String(TOKEN_FIREFOX)
                                                              : QLatin1String(TOKEN_CHROMIUM);
}

// Parses "history::firefox::/path/to/places.sqlite" into (isFirefox, dbPath).
// Returns false if the id doesn't carry a recognized family token.
bool parseItemId(const QString &id, const char *prefix, bool *isFirefoxOut, QString *dbPathOut)
{
    if (!id.startsWith(QLatin1String(prefix)))
        return false;
    const QString rest = id.mid(qstrlen(prefix));
    if (rest.startsWith(QLatin1String(TOKEN_FIREFOX))) {
        *isFirefoxOut = true;
        *dbPathOut = rest.mid(qstrlen(TOKEN_FIREFOX));
        return true;
    }
    if (rest.startsWith(QLatin1String(TOKEN_CHROMIUM))) {
        *isFirefoxOut = false;
        *dbPathOut = rest.mid(qstrlen(TOKEN_CHROMIUM));
        return true;
    }
    return false;
}

QString uniqueConnectionName(const QString &tag)
{
    return QStringLiteral("browser-cleaner-") + tag + QLatin1Char('-') + QUuid::createUuid().toString();
}

// Opens dbPath read-only under a scoped, uniquely-named connection so callers
// never share or leak a global QSqlDatabase handle.
class ScopedReadOnlyDb
{
public:
    explicit ScopedReadOnlyDb(const QString &dbPath)
        : mConnName(uniqueConnectionName(QStringLiteral("ro")))
    {
        mDb = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), mConnName);
        mDb.setDatabaseName(dbPath);
        mDb.setConnectOptions(QStringLiteral("QSQLITE_OPEN_READONLY"));
        mOk = mDb.open();
    }
    ~ScopedReadOnlyDb()
    {
        mDb.close();
        mDb = QSqlDatabase();
        QSqlDatabase::removeDatabase(mConnName);
    }
    bool isOpen() const { return mOk; }
    QSqlDatabase &db() { return mDb; }

private:
    QString mConnName;
    QSqlDatabase mDb;
    bool mOk = false;
};

// Opens dbPath read-write under a scoped, uniquely-named connection.
class ScopedReadWriteDb
{
public:
    explicit ScopedReadWriteDb(const QString &dbPath)
        : mConnName(uniqueConnectionName(QStringLiteral("rw")))
    {
        mDb = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), mConnName);
        mDb.setDatabaseName(dbPath);
        // Fail fast rather than blocking: a busy timeout here would just
        // delay the "in use" verdict, not avoid it.
        mDb.setConnectOptions(QStringLiteral("QSQLITE_BUSY_TIMEOUT=0"));
        mOk = mDb.open();
    }
    ~ScopedReadWriteDb()
    {
        mDb.close();
        mDb = QSqlDatabase();
        QSqlDatabase::removeDatabase(mConnName);
    }
    bool isOpen() const { return mOk; }
    QSqlDatabase &db() { return mDb; }
    QString lastError() const { return mDb.lastError().text(); }

private:
    QString mConnName;
    QSqlDatabase mDb;
    bool mOk = false;
};

// Probes for a write lock by grabbing and immediately releasing a RESERVED
// lock (BEGIN IMMEDIATE). If a running browser instance still holds a write
// lock on this file, SQLite reports "database is locked"/"database is busy"
// here instead of letting a real DELETE run into it mid-transaction.
bool tryProbeWriteLock(QSqlDatabase &db, QString *errorOut)
{
    QSqlQuery probe(db);
    if (!probe.exec(QStringLiteral("BEGIN IMMEDIATE"))) {
        if (errorOut)
            *errorOut = probe.lastError().text();
        return false;
    }
    probe.exec(QStringLiteral("ROLLBACK"));
    return true;
}

qint64 countRows(QSqlDatabase &db, const QString &sql)
{
    QSqlQuery q(db);
    if (!q.exec(sql) || !q.next())
        return -1;
    return q.value(0).toLongLong();
}

// Strips a leading "." — both Firefox (moz_cookies.host) and Chromium
// (cookies.host_key) prefix domain-wide cookies with a dot.
QString normalizedHost(const QString &host)
{
    return host.startsWith(QLatin1Char('.')) ? host.mid(1) : host;
}

} // namespace

BrowserSqliteCleaner::BrowserSqliteCleaner(QList<BrowserProfileLocator::Profile> profiles,
                                            QSet<QString> keptCookieDomains)
    : mProfiles(std::move(profiles))
    , mKeptCookieDomains(std::move(keptCookieDomains))
{
}

bool BrowserSqliteCleaner::isCookieKept(const QString &cookieHost, const QSet<QString> &keptCookieDomains)
{
    const QString host = normalizedHost(cookieHost).toLower();
    for (const QString &kept : keptCookieDomains) {
        const QString keptDomain = normalizedHost(kept).toLower();
        if (keptDomain.isEmpty())
            continue;
        if (host == keptDomain || host.endsWith(QLatin1Char('.') + keptDomain))
            return true;
    }
    return false;
}

void BrowserSqliteCleaner::scan(QAtomicInt *cancelled,
                                 const std::function<void(const TrustSafetyActionItem &)> &itemFound)
{
    for (const BrowserProfileLocator::Profile &profile : mProfiles) {
        if (cancelled && cancelled->loadRelaxed())
            return;

        if (!profile.historyDbPath.isEmpty())
            scanHistory(profile, itemFound);
        if (cancelled && cancelled->loadRelaxed())
            return;
        if (!profile.cookiesDbPath.isEmpty())
            scanCookies(profile, itemFound);
    }
}

void BrowserSqliteCleaner::scanHistory(
    const BrowserProfileLocator::Profile &profile,
    const std::function<void(const TrustSafetyActionItem &)> &itemFound)
{
    const bool isFirefox = profile.family == BrowserProfileLocator::Family::Firefox;
    const QString urlsTable = isFirefox ? QStringLiteral("moz_places") : QStringLiteral("urls");
    const QString visitsTable = isFirefox ? QStringLiteral("moz_historyvisits") : QStringLiteral("visits");
    // Firefox stores bookmarked URLs as moz_places rows referenced by
    // moz_bookmarks.fk, including URLs with zero visits that exist only
    // because they're bookmarked — those must survive a "history" delete.
    const QString urlsWhereClause = isFirefox
        ? QStringLiteral(" WHERE id NOT IN (SELECT fk FROM moz_bookmarks WHERE fk IS NOT NULL)")
        : QString();

    qint64 siteCount = -1;
    qint64 visitCount = -1;
    {
        ScopedReadOnlyDb ro(profile.historyDbPath);
        if (!ro.isOpen())
            return;
        siteCount = countRows(ro.db(), QStringLiteral("SELECT COUNT(*) FROM %1%2").arg(urlsTable, urlsWhereClause));
        visitCount = countRows(ro.db(), QStringLiteral("SELECT COUNT(*) FROM %1").arg(visitsTable));
    }
    if (siteCount <= 0 && visitCount <= 0)
        return;

    TrustSafetyActionItem item;
    item.id = QLatin1String(ID_PREFIX_HISTORY) + familyToken(profile.family) + profile.historyDbPath;
    item.label = QStringLiteral("%1 — %2 — Browsing History").arg(profile.browserName, profile.profileName);
    item.description = QObject::tr("%1 history entries across %2 sites will be deleted.")
                            .arg(visitCount < 0 ? 0 : visitCount)
                            .arg(siteCount < 0 ? 0 : siteCount);
    item.command = QStringLiteral("DELETE FROM %1; DELETE FROM %2%3;").arg(visitsTable, urlsTable, urlsWhereClause);
    item.categoryId = QStringLiteral("browser_deep_clean_history");
    item.categoryLabel = QObject::tr("Browser History");
    item.riskTier = TrustSafetyActionItem::RiskTier::Standard;
    item.estimatedSizeBytes = static_cast<qint64>(FileUtil::getFileSize(profile.historyDbPath));
    itemFound(item);
}

void BrowserSqliteCleaner::scanCookies(
    const BrowserProfileLocator::Profile &profile,
    const std::function<void(const TrustSafetyActionItem &)> &itemFound)
{
    const bool isFirefox = profile.family == BrowserProfileLocator::Family::Firefox;
    const QString cookiesTable = isFirefox ? QStringLiteral("moz_cookies") : QStringLiteral("cookies");
    const QString hostColumn = isFirefox ? QStringLiteral("host") : QStringLiteral("host_key");

    QStringList deletedDomains;
    qint64 totalCookies = 0;
    qint64 keptCookies = 0;
    qint64 deletedCookies = 0;
    {
        ScopedReadOnlyDb ro(profile.cookiesDbPath);
        if (!ro.isOpen())
            return;

        QSqlQuery q(ro.db());
        if (!q.exec(QStringLiteral("SELECT %1, COUNT(*) FROM %2 GROUP BY %1").arg(hostColumn, cookiesTable)))
            return;

        while (q.next()) {
            const QString host = q.value(0).toString();
            const qint64 count = q.value(1).toLongLong();
            totalCookies += count;
            if (isCookieKept(host, mKeptCookieDomains)) {
                keptCookies += count;
            } else {
                deletedCookies += count;
                deletedDomains << host;
            }
        }
    }
    if (totalCookies == 0)
        return;

    QString domainList;
    if (deletedDomains.size() > kMaxDomainsInDescription) {
        domainList = deletedDomains.mid(0, kMaxDomainsInDescription).join(QStringLiteral(", "))
            + QObject::tr(", +%1 more").arg(deletedDomains.size() - kMaxDomainsInDescription);
    } else {
        domainList = deletedDomains.join(QStringLiteral(", "));
    }

    TrustSafetyActionItem item;
    item.id = QLatin1String(ID_PREFIX_COOKIES) + familyToken(profile.family) + profile.cookiesDbPath;
    item.label = QStringLiteral("%1 — %2 — Cookies").arg(profile.browserName, profile.profileName);
    item.description = keptCookies > 0
        ? QObject::tr("%1 cookies across %2 domains will be deleted (%3 kept: %4).")
              .arg(deletedCookies).arg(deletedDomains.size()).arg(keptCookies).arg(domainList)
        : QObject::tr("%1 cookies across %2 domains will be deleted.")
              .arg(deletedCookies).arg(deletedDomains.size());
    item.command = QStringLiteral("DELETE FROM %1 WHERE %2 NOT IN (kept-domain allowlist);")
                       .arg(cookiesTable, hostColumn);
    item.categoryId = QStringLiteral("browser_deep_clean_cookies");
    item.categoryLabel = QObject::tr("Browser Cookies");
    item.riskTier = TrustSafetyActionItem::RiskTier::Risky;
    item.estimatedSizeBytes = static_cast<qint64>(FileUtil::getFileSize(profile.cookiesDbPath));
    itemFound(item);
}

TrustSafetyActionResult BrowserSqliteCleaner::performItem(const TrustSafetyActionItem &item, bool dryRun)
{
    bool isFirefox = false;
    QString dbPath;

    if (parseItemId(item.id, ID_PREFIX_HISTORY, &isFirefox, &dbPath))
        return deleteHistory(dbPath, isFirefox, dryRun);

    if (parseItemId(item.id, ID_PREFIX_COOKIES, &isFirefox, &dbPath))
        return deleteCookies(dbPath, isFirefox, dryRun);

    TrustSafetyActionResult result;
    result.itemId = item.id;
    result.error = QObject::tr("Unknown item id: %1").arg(item.id);
    return result;
}

TrustSafetyActionResult BrowserSqliteCleaner::deleteHistory(const QString &dbPath, bool isFirefox, bool dryRun)
{
    TrustSafetyActionResult result;
    result.itemId = QLatin1String(ID_PREFIX_HISTORY) + (isFirefox ? QLatin1String(TOKEN_FIREFOX) : QLatin1String(TOKEN_CHROMIUM)) + dbPath;

    const QString urlsTable = isFirefox ? QStringLiteral("moz_places") : QStringLiteral("urls");
    const QString visitsTable = isFirefox ? QStringLiteral("moz_historyvisits") : QStringLiteral("visits");
    // Keep moz_places rows a bookmark still points to — see scanHistory.
    const QString urlsWhereClause = isFirefox
        ? QStringLiteral(" WHERE id NOT IN (SELECT fk FROM moz_bookmarks WHERE fk IS NOT NULL)")
        : QString();

    if (dryRun) {
        result.succeeded = QFileInfo::exists(dbPath);
        result.bytesFreed = 0;
        return result;
    }

    const qint64 sizeBefore = static_cast<qint64>(FileUtil::getFileSize(dbPath));

    ScopedReadWriteDb rw(dbPath);
    if (!rw.isOpen()) {
        result.error = rw.lastError();
        return result;
    }

    QString lockError;
    if (!tryProbeWriteLock(rw.db(), &lockError)) {
        result.error = QObject::tr("%1 is currently in use by a running browser — close it and try again.")
                            .arg(QFileInfo(dbPath).fileName());
        return result;
    }

    QSqlQuery q(rw.db());
    if (!q.exec(QStringLiteral("BEGIN IMMEDIATE"))) {
        result.error = QObject::tr("%1 is currently in use by a running browser — close it and try again.")
                            .arg(QFileInfo(dbPath).fileName());
        return result;
    }
    const bool deletedVisits = q.exec(QStringLiteral("DELETE FROM %1").arg(visitsTable));
    const bool deletedUrls = deletedVisits && q.exec(QStringLiteral("DELETE FROM %1%2").arg(urlsTable, urlsWhereClause));
    if (!deletedUrls) {
        result.error = q.lastError().text();
        q.exec(QStringLiteral("ROLLBACK"));
        return result;
    }
    if (!q.exec(QStringLiteral("COMMIT"))) {
        result.error = q.lastError().text();
        q.exec(QStringLiteral("ROLLBACK"));
        return result;
    }
    if (!q.exec(QStringLiteral("VACUUM"))) {
        // Rows are already gone; VACUUM failing just means the freed space
        // isn't reclaimed yet — not a reason to report the whole op failed.
        result.succeeded = true;
        result.bytesFreed = 0;
        return result;
    }

    result.succeeded = true;
    const qint64 sizeAfter = static_cast<qint64>(FileUtil::getFileSize(dbPath));
    result.bytesFreed = std::max<qint64>(0, sizeBefore - sizeAfter);
    return result;
}

TrustSafetyActionResult BrowserSqliteCleaner::deleteCookies(const QString &dbPath, bool isFirefox, bool dryRun)
{
    TrustSafetyActionResult result;
    result.itemId = QLatin1String(ID_PREFIX_COOKIES) + (isFirefox ? QLatin1String(TOKEN_FIREFOX) : QLatin1String(TOKEN_CHROMIUM)) + dbPath;

    const QString cookiesTable = isFirefox ? QStringLiteral("moz_cookies") : QStringLiteral("cookies");
    const QString hostColumn = isFirefox ? QStringLiteral("host") : QStringLiteral("host_key");
    // SQLite gives every ordinary table an implicit rowid unless declared
    // WITHOUT ROWID; both moz_cookies and Chromium's cookies table qualify.
    const QString idColumn = QStringLiteral("rowid");

    if (dryRun) {
        result.succeeded = QFileInfo::exists(dbPath);
        result.bytesFreed = 0;
        return result;
    }

    const qint64 sizeBefore = static_cast<qint64>(FileUtil::getFileSize(dbPath));

    ScopedReadWriteDb rw(dbPath);
    if (!rw.isOpen()) {
        result.error = rw.lastError();
        return result;
    }

    QString lockError;
    if (!tryProbeWriteLock(rw.db(), &lockError)) {
        result.error = QObject::tr("%1 is currently in use by a running browser — close it and try again.")
                            .arg(QFileInfo(dbPath).fileName());
        return result;
    }

    QSqlQuery beginQ(rw.db());
    if (!beginQ.exec(QStringLiteral("BEGIN IMMEDIATE"))) {
        result.error = QObject::tr("%1 is currently in use by a running browser — close it and try again.")
                            .arg(QFileInfo(dbPath).fileName());
        return result;
    }

    // The keeper allowlist needs subdomain matching (isCookieKept), which
    // SQL can't express directly — read every (rowid, host), decide in C++,
    // then delete the non-kept rowids by id. Cookie tables are small enough
    // (thousands, not millions of rows) for this to be cheap.
    QList<qint64> idsToDelete;
    {
        QSqlQuery q(rw.db());
        if (!q.exec(QStringLiteral("SELECT %1, %2 FROM %3").arg(idColumn, hostColumn, cookiesTable))) {
            result.error = q.lastError().text();
            rw.db().exec(QStringLiteral("ROLLBACK"));
            return result;
        }
        while (q.next()) {
            const qint64 rowId = q.value(0).toLongLong();
            const QString host = q.value(1).toString();
            if (!isCookieKept(host, mKeptCookieDomains))
                idsToDelete << rowId;
        }
    }

    if (!idsToDelete.isEmpty()) {
        QSqlQuery del(rw.db());
        del.prepare(QStringLiteral("DELETE FROM %1 WHERE %2 = ?").arg(cookiesTable, idColumn));
        for (const qint64 rowId : idsToDelete) {
            del.addBindValue(rowId);
            if (!del.exec()) {
                result.error = del.lastError().text();
                rw.db().exec(QStringLiteral("ROLLBACK"));
                return result;
            }
        }
    }

    QSqlQuery commit(rw.db());
    if (!commit.exec(QStringLiteral("COMMIT"))) {
        result.error = commit.lastError().text();
        commit.exec(QStringLiteral("ROLLBACK"));
        return result;
    }

    QSqlQuery vacuum(rw.db());
    if (!vacuum.exec(QStringLiteral("VACUUM"))) {
        result.succeeded = true;
        result.bytesFreed = 0;
        return result;
    }

    result.succeeded = true;
    const qint64 sizeAfter = static_cast<qint64>(FileUtil::getFileSize(dbPath));
    result.bytesFreed = std::max<qint64>(0, sizeBefore - sizeAfter);
    return result;
}
