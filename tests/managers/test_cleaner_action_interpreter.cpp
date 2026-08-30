// SSO-23859: CleanerActionInterpreter — executes parsed CleanerML actions
// (delete/glob/walk/regex/truncate/sqlite.vacuum) against a sandboxed
// home/cache root, with a dry-run mode that never touches disk.

#include <QTest>
#include <QDir>
#include <QFile>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QUuid>

#include "cleaner_action_interpreter.h"

using namespace CleanerML;

namespace {

Action makeAction(ActionType type, const QString &path, const QString &regex = QString(),
                   const QString &search = QString())
{
    Action a;
    a.type = type;
    a.path = path;
    a.regex = regex;
    a.search = search;
    return a;
}

Cleaner makeCleaner(const QString &optionId, const QList<Action> &actions)
{
    Option option;
    option.id = optionId;
    option.label = optionId;
    option.actions = actions;

    Cleaner cleaner;
    cleaner.id = QStringLiteral("test-cleaner");
    cleaner.label = QStringLiteral("Test Cleaner");
    cleaner.options = {option};
    return cleaner;
}

void writeFile(const QString &path, const QByteArray &content = "x")
{
    QDir().mkpath(QFileInfo(path).path());
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(content);
    f.close();
}

} // namespace

class TestCleanerActionInterpreter : public QObject
{
    Q_OBJECT

private slots:
    void init();

    void delete_dryRun_reportsSizeWithoutRemoving();
    void delete_liveRun_removesFile();
    void glob_matchesAndDeletesMultipleFiles();
    void walk_recursesIntoSubdirectories();
    void regex_filtersByPattern();
    void truncate_dryRun_reportsCurrentSizeWithoutTruncating();
    void truncate_liveRun_zeroesFile();
    void sqliteVacuum_liveRun_shrinksFixtureDatabase();
    void sqliteVacuum_dryRun_doesNotModifyDatabase();
    void unresolvableVariable_isSkippedEntirely();
    void pathEscapingSandbox_isSkippedEntirely();
    void unselectedOption_isNotScanned();

private:
    QScopedPointer<QTemporaryDir> mTmp;
    QString mHome;
    QString mCache;

    CleanerActionInterpreter::SandboxRoots roots() const;
    QList<TrustSafetyActionItem> collect(CleanerActionInterpreter &interpreter) const;
};

void TestCleanerActionInterpreter::init()
{
    mTmp.reset(new QTemporaryDir());
    QVERIFY(mTmp->isValid());
    mHome = mTmp->filePath("home");
    mCache = mTmp->filePath("cache");
    QVERIFY(QDir().mkpath(mHome));
    QVERIFY(QDir().mkpath(mCache));
}

CleanerActionInterpreter::SandboxRoots TestCleanerActionInterpreter::roots() const
{
    CleanerActionInterpreter::SandboxRoots r;
    r.home = mHome;
    r.cache = mCache;
    return r;
}

QList<TrustSafetyActionItem> TestCleanerActionInterpreter::collect(CleanerActionInterpreter &interpreter) const
{
    QList<TrustSafetyActionItem> items;
    interpreter.scan(nullptr, [&items](const TrustSafetyActionItem &item) { items.append(item); });
    return items;
}

void TestCleanerActionInterpreter::delete_dryRun_reportsSizeWithoutRemoving()
{
    writeFile(mHome + "/notes.txt", QByteArray(42, 'a'));

    Cleaner cleaner = makeCleaner("opt", {makeAction(ActionType::Delete, "$$home$$/notes.txt")});
    CleanerActionInterpreter interpreter(cleaner, {"opt"}, roots());

    const auto items = collect(interpreter);
    QCOMPARE(items.size(), 1);
    QCOMPARE(items.first().estimatedSizeBytes, qint64(42));

    const auto result = interpreter.performItem(items.first(), /*dryRun=*/true);
    QVERIFY(result.succeeded);
    QCOMPARE(result.bytesFreed, qint64(42));
    QVERIFY(QFileInfo::exists(mHome + "/notes.txt"));
}

void TestCleanerActionInterpreter::delete_liveRun_removesFile()
{
    writeFile(mHome + "/notes.txt");

    Cleaner cleaner = makeCleaner("opt", {makeAction(ActionType::Delete, "$$home$$/notes.txt")});
    CleanerActionInterpreter interpreter(cleaner, {"opt"}, roots());

    const auto items = collect(interpreter);
    QCOMPARE(items.size(), 1);

    const auto result = interpreter.performItem(items.first(), /*dryRun=*/false);
    QVERIFY(result.succeeded);
    QVERIFY(!QFileInfo::exists(mHome + "/notes.txt"));
}

void TestCleanerActionInterpreter::glob_matchesAndDeletesMultipleFiles()
{
    writeFile(mCache + "/logs/a.log");
    writeFile(mCache + "/logs/b.log");
    writeFile(mCache + "/logs/keep.txt");

    Cleaner cleaner = makeCleaner("opt", {makeAction(ActionType::Glob, "$$cache$$/logs/*.log", QString(), "glob")});
    CleanerActionInterpreter interpreter(cleaner, {"opt"}, roots());

    const auto items = collect(interpreter);
    QCOMPARE(items.size(), 2);

    for (const auto &item : items)
        QVERIFY(interpreter.performItem(item, /*dryRun=*/false).succeeded);

    QVERIFY(!QFileInfo::exists(mCache + "/logs/a.log"));
    QVERIFY(!QFileInfo::exists(mCache + "/logs/b.log"));
    QVERIFY(QFileInfo::exists(mCache + "/logs/keep.txt"));
}

void TestCleanerActionInterpreter::walk_recursesIntoSubdirectories()
{
    writeFile(mCache + "/tmp/top.cache");
    writeFile(mCache + "/tmp/nested/deep.cache");

    Cleaner cleaner = makeCleaner("opt", {makeAction(ActionType::Walk, "$$cache$$/tmp")});
    CleanerActionInterpreter interpreter(cleaner, {"opt"}, roots());

    const auto items = collect(interpreter);
    QCOMPARE(items.size(), 2);
}

void TestCleanerActionInterpreter::regex_filtersByPattern()
{
    writeFile(mHome + "/App/logs/app.log");
    writeFile(mHome + "/App/logs/app.log.1");
    writeFile(mHome + "/App/logs/keep.txt");

    Cleaner cleaner = makeCleaner(
        "opt", {makeAction(ActionType::Regex, "$$home$$/App/logs", R"(\.log(\.\d+)?$)")});
    CleanerActionInterpreter interpreter(cleaner, {"opt"}, roots());

    const auto items = collect(interpreter);
    QCOMPARE(items.size(), 2);
}

void TestCleanerActionInterpreter::truncate_dryRun_reportsCurrentSizeWithoutTruncating()
{
    writeFile(mHome + "/big.dat", QByteArray(100, 'x'));

    Cleaner cleaner = makeCleaner("opt", {makeAction(ActionType::Truncate, "$$home$$/big.dat")});
    CleanerActionInterpreter interpreter(cleaner, {"opt"}, roots());

    const auto items = collect(interpreter);
    QCOMPARE(items.size(), 1);
    QCOMPARE(items.first().estimatedSizeBytes, qint64(100));

    const auto result = interpreter.performItem(items.first(), /*dryRun=*/true);
    QVERIFY(result.succeeded);
    QCOMPARE(QFileInfo(mHome + "/big.dat").size(), qint64(100));
}

void TestCleanerActionInterpreter::truncate_liveRun_zeroesFile()
{
    writeFile(mHome + "/big.dat", QByteArray(100, 'x'));

    Cleaner cleaner = makeCleaner("opt", {makeAction(ActionType::Truncate, "$$home$$/big.dat")});
    CleanerActionInterpreter interpreter(cleaner, {"opt"}, roots());

    const auto items = collect(interpreter);
    const auto result = interpreter.performItem(items.first(), /*dryRun=*/false);
    QVERIFY(result.succeeded);
    QCOMPARE(result.bytesFreed, qint64(100));
    QVERIFY(QFileInfo::exists(mHome + "/big.dat"));
    QCOMPARE(QFileInfo(mHome + "/big.dat").size(), qint64(0));
}

void TestCleanerActionInterpreter::sqliteVacuum_liveRun_shrinksFixtureDatabase()
{
    const QString dbPath = mCache + "/app.sqlite";
    const QString connName = QStringLiteral("fixture-") + QUuid::createUuid().toString();
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connName);
        db.setDatabaseName(dbPath);
        QVERIFY(db.open());
        QSqlQuery q(db);
        QVERIFY(q.exec(QStringLiteral("CREATE TABLE junk(data TEXT)")));
        for (int i = 0; i < 500; ++i)
            QVERIFY(q.exec(QStringLiteral("INSERT INTO junk(data) VALUES ('%1')").arg(QString(200, 'a'))));
        QVERIFY(q.exec(QStringLiteral("DELETE FROM junk")));
        db.close();
    }
    QSqlDatabase::removeDatabase(connName);

    const qint64 sizeBeforeVacuum = QFileInfo(dbPath).size();
    QVERIFY(sizeBeforeVacuum > 0);

    Cleaner cleaner = makeCleaner(
        "opt", {makeAction(ActionType::SqliteVacuum, "$$cache$$/app.sqlite", QString(), "file")});
    CleanerActionInterpreter interpreter(cleaner, {"opt"}, roots());

    const auto items = collect(interpreter);
    QCOMPARE(items.size(), 1);

    const auto result = interpreter.performItem(items.first(), /*dryRun=*/false);
    QVERIFY2(result.succeeded, qPrintable(result.error));
    QVERIFY(result.bytesFreed > 0);
    QVERIFY(QFileInfo(dbPath).size() < sizeBeforeVacuum);
}

void TestCleanerActionInterpreter::sqliteVacuum_dryRun_doesNotModifyDatabase()
{
    const QString dbPath = mCache + "/app.sqlite";
    const QString connName = QStringLiteral("fixture-") + QUuid::createUuid().toString();
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connName);
        db.setDatabaseName(dbPath);
        QVERIFY(db.open());
        QSqlQuery q(db);
        QVERIFY(q.exec(QStringLiteral("CREATE TABLE junk(data TEXT)")));
        QVERIFY(q.exec(QStringLiteral("INSERT INTO junk(data) VALUES ('keep-me')")));
        db.close();
    }
    QSqlDatabase::removeDatabase(connName);

    const qint64 sizeBefore = QFileInfo(dbPath).size();

    Cleaner cleaner = makeCleaner(
        "opt", {makeAction(ActionType::SqliteVacuum, "$$cache$$/app.sqlite", QString(), "file")});
    CleanerActionInterpreter interpreter(cleaner, {"opt"}, roots());

    const auto items = collect(interpreter);
    const auto result = interpreter.performItem(items.first(), /*dryRun=*/true);
    QVERIFY(result.succeeded);
    QCOMPARE(QFileInfo(dbPath).size(), sizeBefore);

    // Row is still there — dry run must not touch the database at all.
    const QString verifyConn = QStringLiteral("verify-") + QUuid::createUuid().toString();
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), verifyConn);
        db.setDatabaseName(dbPath);
        QVERIFY(db.open());
        QSqlQuery q(db);
        QVERIFY(q.exec(QStringLiteral("SELECT COUNT(*) FROM junk")));
        QVERIFY(q.next());
        QCOMPARE(q.value(0).toInt(), 1);
        db.close();
    }
    QSqlDatabase::removeDatabase(verifyConn);
}

void TestCleanerActionInterpreter::unresolvableVariable_isSkippedEntirely()
{
    // $$profile$$ is an app-specific browser-profile token this generic
    // interpreter does not resolve (SSO-23860 scope) — the action must be
    // dropped at scan() time, not guessed at.
    Cleaner cleaner = makeCleaner("opt", {makeAction(ActionType::Delete, "$$profile$$/cookies.sqlite")});
    CleanerActionInterpreter interpreter(cleaner, {"opt"}, roots());

    QVERIFY(collect(interpreter).isEmpty());
}

void TestCleanerActionInterpreter::pathEscapingSandbox_isSkippedEntirely()
{
    // A crafted traversal riding on a legitimately-resolved $$home$$ token
    // must not escape the sandbox (SSO-23859 AC).
    Cleaner cleaner = makeCleaner(
        "opt", {makeAction(ActionType::Delete, "$$home$$/../../../../../../etc/passwd")});
    CleanerActionInterpreter interpreter(cleaner, {"opt"}, roots());

    QVERIFY(collect(interpreter).isEmpty());
}

void TestCleanerActionInterpreter::unselectedOption_isNotScanned()
{
    writeFile(mHome + "/notes.txt");

    Cleaner cleaner = makeCleaner("opt", {makeAction(ActionType::Delete, "$$home$$/notes.txt")});
    CleanerActionInterpreter interpreter(cleaner, /*selectedOptionIds=*/{}, roots());

    QVERIFY(collect(interpreter).isEmpty());
}

QTEST_MAIN(TestCleanerActionInterpreter)
#include "test_cleaner_action_interpreter.moc"
