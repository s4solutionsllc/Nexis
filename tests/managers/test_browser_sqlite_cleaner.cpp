// SSO-23860: BrowserSqliteCleaner — surgical Firefox/Chromium history &
// cookie row deletion + VACUUM, with a selective cookie keeper allowlist and
// a refusal when the target DB is write-locked by another connection
// (standing in for a running browser instance).

#include <QTest>
#include <QDir>
#include <QFile>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QUuid>

#include "browser_sqlite_cleaner.h"

using Family = BrowserProfileLocator::Family;
using Profile = BrowserProfileLocator::Profile;

namespace {

QString newConnection(const QString &tag)
{
    return tag + QLatin1Char('-') + QUuid::createUuid().toString();
}

void execOrFail(QSqlQuery &q, const QString &sql)
{
    QVERIFY2(q.exec(sql), qPrintable(q.lastError().text() + " :: " + sql));
}

void createFirefoxFixture(const QString &placesPath, const QString &cookiesPath)
{
    const QString placesConn = newConnection("fx-places");
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), placesConn);
        db.setDatabaseName(placesPath);
        QVERIFY(db.open());
        QSqlQuery q(db);
        execOrFail(q, QStringLiteral("CREATE TABLE moz_places(id INTEGER PRIMARY KEY, url TEXT)"));
        execOrFail(q, QStringLiteral("CREATE TABLE moz_historyvisits(id INTEGER PRIMARY KEY, place_id INTEGER)"));
        // Real Firefox profiles always have this table; kept empty here so
        // existing fixture rows (none bookmarked) behave exactly as before.
        execOrFail(q, QStringLiteral("CREATE TABLE moz_bookmarks(id INTEGER PRIMARY KEY, fk INTEGER)"));
        for (int i = 0; i < 5; ++i) {
            execOrFail(q, QStringLiteral("INSERT INTO moz_places(url) VALUES ('https://site%1.example/')").arg(i));
            execOrFail(q, QStringLiteral("INSERT INTO moz_historyvisits(place_id) VALUES (%1)").arg(i + 1));
            execOrFail(q, QStringLiteral("INSERT INTO moz_historyvisits(place_id) VALUES (%1)").arg(i + 1));
        }
        db.close();
    }
    QSqlDatabase::removeDatabase(placesConn);

    const QString cookiesConn = newConnection("fx-cookies");
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), cookiesConn);
        db.setDatabaseName(cookiesPath);
        QVERIFY(db.open());
        QSqlQuery q(db);
        execOrFail(q, QStringLiteral("CREATE TABLE moz_cookies(id INTEGER PRIMARY KEY, host TEXT, name TEXT, value TEXT)"));
        execOrFail(q, QStringLiteral("INSERT INTO moz_cookies(host, name, value) VALUES ('.example.com', 'sid', 'a')"));
        execOrFail(q, QStringLiteral("INSERT INTO moz_cookies(host, name, value) VALUES ('.example.com', 'pref', 'b')"));
        execOrFail(q, QStringLiteral("INSERT INTO moz_cookies(host, name, value) VALUES ('bank.example.org', 'tok', 'c')"));
        execOrFail(q, QStringLiteral("INSERT INTO moz_cookies(host, name, value) VALUES ('tracker.ads.example', 'uid', 'd')"));
        db.close();
    }
    QSqlDatabase::removeDatabase(cookiesConn);
}

void createChromiumFixture(const QString &historyPath, const QString &cookiesPath)
{
    const QString historyConn = newConnection("cr-history");
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), historyConn);
        db.setDatabaseName(historyPath);
        QVERIFY(db.open());
        QSqlQuery q(db);
        execOrFail(q, QStringLiteral("CREATE TABLE urls(id INTEGER PRIMARY KEY, url TEXT)"));
        execOrFail(q, QStringLiteral("CREATE TABLE visits(id INTEGER PRIMARY KEY, url INTEGER)"));
        execOrFail(q, QStringLiteral("INSERT INTO urls(url) VALUES ('https://a.example/')"));
        execOrFail(q, QStringLiteral("INSERT INTO visits(url) VALUES (1)"));
        db.close();
    }
    QSqlDatabase::removeDatabase(historyConn);

    const QString cookiesConn = newConnection("cr-cookies");
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), cookiesConn);
        db.setDatabaseName(cookiesPath);
        QVERIFY(db.open());
        QSqlQuery q(db);
        execOrFail(q, QStringLiteral("CREATE TABLE cookies(host_key TEXT, name TEXT, value TEXT)"));
        execOrFail(q, QStringLiteral("INSERT INTO cookies(host_key, name, value) VALUES ('.example.com', 'sid', 'a')"));
        execOrFail(q, QStringLiteral("INSERT INTO cookies(host_key, name, value) VALUES ('ads.example', 'uid', 'b')"));
        db.close();
    }
    QSqlDatabase::removeDatabase(cookiesConn);
}

qint64 rowCount(const QString &dbPath, const QString &table)
{
    const QString conn = newConnection("verify");
    qint64 count = -1;
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), conn);
        db.setDatabaseName(dbPath);
        if (db.open()) {
            QSqlQuery q(db);
            if (q.exec(QStringLiteral("SELECT COUNT(*) FROM %1").arg(table)) && q.next())
                count = q.value(0).toLongLong();
            db.close();
        }
    }
    QSqlDatabase::removeDatabase(conn);
    return count;
}

TrustSafetyActionItem findItem(const QList<TrustSafetyActionItem> &items, const QString &idPrefix)
{
    for (const auto &item : items) {
        if (item.id.startsWith(idPrefix))
            return item;
    }
    return TrustSafetyActionItem();
}

} // namespace

class TestBrowserSqliteCleaner : public QObject
{
    Q_OBJECT

private slots:
    void init();

    void isCookieKept_exactAndSubdomainMatch();
    void isCookieKept_unrelatedDomainNotKept();

    void firefox_scan_reportsHistoryAndCookieCounts();
    void firefox_liveRun_deletesHistoryAndKeepsAllowlistedCookies();
    void firefox_liveRun_preservesBookmarkedPlaceWithZeroVisits();
    void chromium_liveRun_deletesHistoryAndKeepsAllowlistedCookies();

    void performItem_dryRun_neverModifiesDatabase();
    void performItem_refusesWhenCookiesDbIsWriteLocked();

private:
    QScopedPointer<QTemporaryDir> mTmp;
    QString mPlaces, mCookiesFx, mHistoryCr, mCookiesCr;

    QList<TrustSafetyActionItem> collect(BrowserSqliteCleaner &cleaner) const;
};

void TestBrowserSqliteCleaner::init()
{
    mTmp.reset(new QTemporaryDir());
    QVERIFY(mTmp->isValid());
    mPlaces = mTmp->filePath("places.sqlite");
    mCookiesFx = mTmp->filePath("cookies.sqlite");
    mHistoryCr = mTmp->filePath("History");
    mCookiesCr = mTmp->filePath("Cookies");
}

QList<TrustSafetyActionItem> TestBrowserSqliteCleaner::collect(BrowserSqliteCleaner &cleaner) const
{
    QList<TrustSafetyActionItem> items;
    cleaner.scan(nullptr, [&items](const TrustSafetyActionItem &item) { items.append(item); });
    return items;
}

void TestBrowserSqliteCleaner::isCookieKept_exactAndSubdomainMatch()
{
    const QSet<QString> kept = {QStringLiteral("example.com")};
    QVERIFY(BrowserSqliteCleaner::isCookieKept(QStringLiteral("example.com"), kept));
    QVERIFY(BrowserSqliteCleaner::isCookieKept(QStringLiteral(".example.com"), kept));
    QVERIFY(BrowserSqliteCleaner::isCookieKept(QStringLiteral("login.example.com"), kept));
    QVERIFY(BrowserSqliteCleaner::isCookieKept(QStringLiteral("EXAMPLE.COM"), kept));
}

void TestBrowserSqliteCleaner::isCookieKept_unrelatedDomainNotKept()
{
    const QSet<QString> kept = {QStringLiteral("example.com")};
    QVERIFY(!BrowserSqliteCleaner::isCookieKept(QStringLiteral("notexample.com"), kept));
    QVERIFY(!BrowserSqliteCleaner::isCookieKept(QStringLiteral("tracker.ads.example"), kept));
}

void TestBrowserSqliteCleaner::firefox_scan_reportsHistoryAndCookieCounts()
{
    createFirefoxFixture(mPlaces, mCookiesFx);

    Profile profile;
    profile.family = Family::Firefox;
    profile.browserName = QStringLiteral("Firefox");
    profile.profileName = QStringLiteral("default-release");
    profile.historyDbPath = mPlaces;
    profile.cookiesDbPath = mCookiesFx;

    BrowserSqliteCleaner cleaner({profile}, {QStringLiteral("example.com")});
    const auto items = collect(cleaner);
    QCOMPARE(items.size(), 2);

    const auto historyItem = findItem(items, QStringLiteral("history::"));
    QVERIFY(!historyItem.id.isEmpty());
    QVERIFY(historyItem.description.contains(QStringLiteral("10")));  // 10 visits
    QVERIFY(historyItem.description.contains(QStringLiteral("5")));   // 5 sites

    const auto cookieItem = findItem(items, QStringLiteral("cookies::"));
    QVERIFY(!cookieItem.id.isEmpty());
    // 2 cookies deleted (bank.example.org, tracker.ads.example), 2 kept (.example.com).
    QVERIFY(cookieItem.description.contains(QStringLiteral("2 kept")));
}

void TestBrowserSqliteCleaner::firefox_liveRun_deletesHistoryAndKeepsAllowlistedCookies()
{
    createFirefoxFixture(mPlaces, mCookiesFx);

    Profile profile;
    profile.family = Family::Firefox;
    profile.browserName = QStringLiteral("Firefox");
    profile.profileName = QStringLiteral("default-release");
    profile.historyDbPath = mPlaces;
    profile.cookiesDbPath = mCookiesFx;

    BrowserSqliteCleaner cleaner({profile}, {QStringLiteral("example.com")});
    const auto items = collect(cleaner);

    for (const auto &item : items) {
        const auto result = cleaner.performItem(item, /*dryRun=*/false);
        QVERIFY2(result.succeeded, qPrintable(result.error));
    }

    QCOMPARE(rowCount(mPlaces, QStringLiteral("moz_places")), qint64(0));
    QCOMPARE(rowCount(mPlaces, QStringLiteral("moz_historyvisits")), qint64(0));

    QCOMPARE(rowCount(mCookiesFx, QStringLiteral("moz_cookies")), qint64(2));

    const QString conn = newConnection("verify-hosts");
    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), conn);
    db.setDatabaseName(mCookiesFx);
    QVERIFY(db.open());
    QSqlQuery q(db);
    QVERIFY(q.exec(QStringLiteral("SELECT host FROM moz_cookies ORDER BY host")));
    QStringList remaining;
    while (q.next())
        remaining << q.value(0).toString();
    db.close();
    QSqlDatabase::removeDatabase(conn);

    QCOMPARE(remaining.size(), 2);
    for (const QString &host : remaining)
        QVERIFY(host.contains(QStringLiteral("example.com")) && !host.contains(QStringLiteral("bank")));
}

void TestBrowserSqliteCleaner::firefox_liveRun_preservesBookmarkedPlaceWithZeroVisits()
{
    createFirefoxFixture(mPlaces, mCookiesFx);

    // A bookmarked URL with no visits — the exact shape moz_places holds for
    // "bookmarked but never (re)visited" pages. moz_bookmarks.fk points at it.
    const QString conn = newConnection("fx-bookmark-setup");
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), conn);
        db.setDatabaseName(mPlaces);
        QVERIFY(db.open());
        QSqlQuery q(db);
        execOrFail(q, QStringLiteral("INSERT INTO moz_places(url) VALUES ('https://bookmarked.example/')"));
        execOrFail(q, QStringLiteral("INSERT INTO moz_bookmarks(fk) VALUES (last_insert_rowid())"));
        db.close();
    }
    QSqlDatabase::removeDatabase(conn);

    Profile profile;
    profile.family = Family::Firefox;
    profile.browserName = QStringLiteral("Firefox");
    profile.profileName = QStringLiteral("default-release");
    profile.historyDbPath = mPlaces;
    profile.cookiesDbPath = mCookiesFx;

    BrowserSqliteCleaner cleaner({profile}, {});
    const auto items = collect(cleaner);

    // Preview counts the 5 non-bookmarked sites as eligible, not the bookmark.
    const auto historyItem = findItem(items, QStringLiteral("history::"));
    QVERIFY(!historyItem.id.isEmpty());
    QVERIFY(historyItem.description.contains(QStringLiteral("5")));

    for (const auto &item : items) {
        const auto result = cleaner.performItem(item, /*dryRun=*/false);
        QVERIFY2(result.succeeded, qPrintable(result.error));
    }

    // Only the bookmarked row survives; its zero visits are still gone.
    QCOMPARE(rowCount(mPlaces, QStringLiteral("moz_places")), qint64(1));
    QCOMPARE(rowCount(mPlaces, QStringLiteral("moz_historyvisits")), qint64(0));

    const QString verifyConn = newConnection("fx-bookmark-verify");
    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), verifyConn);
    db.setDatabaseName(mPlaces);
    QVERIFY(db.open());
    QSqlQuery q(db);
    QVERIFY(q.exec(QStringLiteral("SELECT url FROM moz_places")));
    QVERIFY(q.next());
    QCOMPARE(q.value(0).toString(), QStringLiteral("https://bookmarked.example/"));
    db.close();
    QSqlDatabase::removeDatabase(verifyConn);
}

void TestBrowserSqliteCleaner::chromium_liveRun_deletesHistoryAndKeepsAllowlistedCookies()
{
    createChromiumFixture(mHistoryCr, mCookiesCr);

    Profile profile;
    profile.family = Family::Chromium;
    profile.browserName = QStringLiteral("Google Chrome");
    profile.profileName = QStringLiteral("Default");
    profile.historyDbPath = mHistoryCr;
    profile.cookiesDbPath = mCookiesCr;

    BrowserSqliteCleaner cleaner({profile}, {QStringLiteral("example.com")});
    const auto items = collect(cleaner);
    QCOMPARE(items.size(), 2);

    for (const auto &item : items) {
        const auto result = cleaner.performItem(item, /*dryRun=*/false);
        QVERIFY2(result.succeeded, qPrintable(result.error));
    }

    QCOMPARE(rowCount(mHistoryCr, QStringLiteral("urls")), qint64(0));
    QCOMPARE(rowCount(mHistoryCr, QStringLiteral("visits")), qint64(0));

    // .example.com kept, ads.example deleted.
    QCOMPARE(rowCount(mCookiesCr, QStringLiteral("cookies")), qint64(1));
}

void TestBrowserSqliteCleaner::performItem_dryRun_neverModifiesDatabase()
{
    createFirefoxFixture(mPlaces, mCookiesFx);

    Profile profile;
    profile.family = Family::Firefox;
    profile.browserName = QStringLiteral("Firefox");
    profile.profileName = QStringLiteral("default-release");
    profile.historyDbPath = mPlaces;
    profile.cookiesDbPath = mCookiesFx;

    BrowserSqliteCleaner cleaner({profile}, {});
    const auto items = collect(cleaner);

    for (const auto &item : items) {
        const auto result = cleaner.performItem(item, /*dryRun=*/true);
        QVERIFY(result.succeeded);
    }

    QCOMPARE(rowCount(mPlaces, QStringLiteral("moz_places")), qint64(5));
    QCOMPARE(rowCount(mCookiesFx, QStringLiteral("moz_cookies")), qint64(4));
}

void TestBrowserSqliteCleaner::performItem_refusesWhenCookiesDbIsWriteLocked()
{
    createFirefoxFixture(mPlaces, mCookiesFx);

    Profile profile;
    profile.family = Family::Firefox;
    profile.browserName = QStringLiteral("Firefox");
    profile.profileName = QStringLiteral("default-release");
    profile.historyDbPath = mPlaces;
    profile.cookiesDbPath = mCookiesFx;

    BrowserSqliteCleaner cleaner({profile}, {});
    const auto items = collect(cleaner);
    const auto cookieItem = findItem(items, QStringLiteral("cookies::"));
    QVERIFY(!cookieItem.id.isEmpty());

    // Simulate a running browser holding a write lock on cookies.sqlite by
    // grabbing a RESERVED lock on another connection and never releasing it
    // for the duration of this test.
    const QString lockConn = newConnection("simulated-browser");
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), lockConn);
        db.setDatabaseName(mCookiesFx);
        QVERIFY(db.open());
        QSqlQuery q(db);
        QVERIFY(q.exec(QStringLiteral("BEGIN IMMEDIATE")));
        execOrFail(q, QStringLiteral("INSERT INTO moz_cookies(host, name, value) VALUES ('live.example', 'x', 'y')"));

        const auto result = cleaner.performItem(cookieItem, /*dryRun=*/false);
        QVERIFY(!result.succeeded);
        QVERIFY(!result.error.isEmpty());

        q.exec(QStringLiteral("ROLLBACK"));
        db.close();
    }
    QSqlDatabase::removeDatabase(lockConn);

    // No corruption, no partial delete: row count unchanged from the fixture.
    QCOMPARE(rowCount(mCookiesFx, QStringLiteral("moz_cookies")), qint64(4));
}

QTEST_MAIN(TestBrowserSqliteCleaner)
#include "test_browser_sqlite_cleaner.moc"
