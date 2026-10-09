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

    // SSO-25782: real-world CleanerML paths.
    void tildeAndXdg_expandInsideTheSandboxHome();
    void unknownEnvironmentVariable_isSkipped();
    void var_expandsToEveryValue();
    void var_globValue_expandsToMatchingDirectories();
    void var_referencingItself_isSkipped();
    void tildeTraversalOutOfHome_isSkipped();
    void sandboxRootItself_isNeverATarget();
    void excludedPath_isNotListed_andRefusedIfForced();
    void optionWarning_makesItemsRisky();

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

void TestCleanerActionInterpreter::tildeAndXdg_expandInsideTheSandboxHome()
{
    writeFile(mHome + "/.bash_history");
    writeFile(mHome + "/.config/app/recent");
    writeFile(mHome + "/.local/share/app/log");
    writeFile(mCache + "/app/thumb");

    Cleaner cleaner = makeCleaner("opt", {
        makeAction(ActionType::Delete, "~/.bash_history"),
        makeAction(ActionType::Delete, "$XDG_CONFIG_HOME/app/recent"),
        makeAction(ActionType::Delete, "$XDG_DATA_HOME/app/log"),
        makeAction(ActionType::Delete, "$XDG_CACHE_HOME/app/thumb"),
        makeAction(ActionType::Delete, "$HOME/.bash_history"),
    });
    CleanerActionInterpreter interpreter(cleaner, {"opt"}, roots());

    QStringList ids;
    for (const auto &item : collect(interpreter))
        ids << item.id;
    QCOMPARE(ids, (QStringList{
        "delete::" + mHome + "/.bash_history",
        "delete::" + mHome + "/.config/app/recent",
        "delete::" + mHome + "/.local/share/app/log",
        "delete::" + mCache + "/app/thumb",
        "delete::" + mHome + "/.bash_history",
    }));
}

void TestCleanerActionInterpreter::unknownEnvironmentVariable_isSkipped()
{
    writeFile(mHome + "/x");
    Cleaner cleaner = makeCleaner("opt", {
        makeAction(ActionType::Delete, "$APPDATA/x"),
        makeAction(ActionType::Delete, "%Temp%/x"),
    });
    CleanerActionInterpreter interpreter(cleaner, {"opt"}, roots());
    QVERIFY(interpreter.expandPath("$APPDATA/x").isEmpty());
    QVERIFY(collect(interpreter).isEmpty());
}

void TestCleanerActionInterpreter::var_expandsToEveryValue()
{
    writeFile(mHome + "/.app/cache.db");
    writeFile(mHome + "/.config/app/cache.db");

    Cleaner cleaner = makeCleaner("opt", {makeAction(ActionType::Delete, "$$Base$$/cache.db")});
    cleaner.vars.insert("base", {VarValue{"~/.app", {}, false},
                                 VarValue{"$XDG_CONFIG_HOME/app", {}, false},
                                 VarValue{"~/.not-installed", {}, false}});
    CleanerActionInterpreter interpreter(cleaner, {"opt"}, roots());

    QCOMPARE(interpreter.expandPath("$$Base$$/cache.db").size(), 3);
    QCOMPARE(collect(interpreter).size(), 2);
}

void TestCleanerActionInterpreter::var_globValue_expandsToMatchingDirectories()
{
    writeFile(mHome + "/.browser/Default/History");
    writeFile(mHome + "/.browser/Profile 1/History");
    writeFile(mHome + "/.browser/Profile 2/History");
    writeFile(mHome + "/.browser/System Profile/History");

    Cleaner cleaner = makeCleaner("opt", {makeAction(ActionType::Delete, "$$profile$$/History")});
    cleaner.vars.insert("profile", {VarValue{"~/.browser/Default", {}, false},
                                    VarValue{"~/.browser/Profile *", {}, true}});
    CleanerActionInterpreter interpreter(cleaner, {"opt"}, roots());

    QStringList ids;
    for (const auto &item : collect(interpreter))
        ids << item.id;
    QCOMPARE(ids, (QStringList{
        "delete::" + mHome + "/.browser/Default/History",
        "delete::" + mHome + "/.browser/Profile 1/History",
        "delete::" + mHome + "/.browser/Profile 2/History",
    }));
}

void TestCleanerActionInterpreter::var_referencingItself_isSkipped()
{
    Cleaner cleaner = makeCleaner("opt", {makeAction(ActionType::Delete, "$$loop$$/x")});
    cleaner.vars.insert("loop", {VarValue{"$$loop$$/again", {}, false}});
    CleanerActionInterpreter interpreter(cleaner, {"opt"}, roots());
    QVERIFY(interpreter.expandPath("$$loop$$/x").isEmpty());
    QVERIFY(collect(interpreter).isEmpty());
}

void TestCleanerActionInterpreter::tildeTraversalOutOfHome_isSkipped()
{
    writeFile(mTmp->filePath("outside/secret"));
    Cleaner cleaner = makeCleaner("opt", {
        makeAction(ActionType::Delete, "~/../outside/secret"),
        makeAction(ActionType::Walk, "~/../outside", QString(), "walk.files"),
    });
    cleaner.vars.insert("escape", {VarValue{"~/../outside", {}, false}});
    cleaner.options.first().actions.append(makeAction(ActionType::Delete, "$$escape$$/secret"));
    CleanerActionInterpreter interpreter(cleaner, {"opt"}, roots());

    QVERIFY(collect(interpreter).isEmpty());
    QVERIFY(QFileInfo::exists(mTmp->filePath("outside/secret")));
}

void TestCleanerActionInterpreter::sandboxRootItself_isNeverATarget()
{
    writeFile(mHome + "/keep.txt");
    Cleaner cleaner = makeCleaner("opt", {
        makeAction(ActionType::Delete, "~"),
        makeAction(ActionType::Delete, "~/"),
        makeAction(ActionType::Delete, "$$home$$"),
        makeAction(ActionType::Delete, "$$cache$$"),
    });
    CleanerActionInterpreter interpreter(cleaner, {"opt"}, roots());
    QVERIFY(collect(interpreter).isEmpty());

    TrustSafetyActionItem forced;
    forced.id = "delete::" + mHome;
    const auto result = interpreter.performItem(forced, /*dryRun=*/false);
    QVERIFY(!result.succeeded);
    QVERIFY(QFileInfo::exists(mHome + "/keep.txt"));
}

void TestCleanerActionInterpreter::excludedPath_isNotListed_andRefusedIfForced()
{
    writeFile(mCache + "/app/keep/a.bin");
    writeFile(mCache + "/app/drop/b.bin");

    Cleaner cleaner = makeCleaner("opt", {makeAction(ActionType::Walk, "$$cache$$/app", QString(), "walk.files")});
    CleanerActionInterpreter interpreter(cleaner, {"opt"}, roots());

    CleanerService::ExclusionEntry entry;
    entry.type = CleanerService::ExclusionEntry::Folder;
    entry.path = mCache + "/app/keep";
    interpreter.setExclusions({entry});

    const auto items = collect(interpreter);
    QCOMPARE(items.size(), 1);
    QVERIFY(items.first().id.endsWith("/app/drop/b.bin"));

    TrustSafetyActionItem forced;
    forced.id = "delete::" + mCache + "/app/keep/a.bin";
    QVERIFY(!interpreter.performItem(forced, /*dryRun=*/false).succeeded);
    QVERIFY(QFileInfo::exists(mCache + "/app/keep/a.bin"));
}

void TestCleanerActionInterpreter::optionWarning_makesItemsRisky()
{
    writeFile(mHome + "/.app/passwords.db");
    writeFile(mHome + "/.app/cache.db");

    Cleaner cleaner = makeCleaner("passwords", {makeAction(ActionType::Delete, "~/.app/passwords.db")});
    cleaner.options.first().warning = "This will delete your saved passwords.";
    Option plain;
    plain.id = "cache";
    plain.label = "Cache";
    plain.actions = {makeAction(ActionType::Delete, "~/.app/cache.db")};
    cleaner.options.append(plain);

    CleanerActionInterpreter interpreter(cleaner, {"passwords", "cache"}, roots());
    const auto items = collect(interpreter);
    QCOMPARE(items.size(), 2);
    QVERIFY(items.at(0).riskTier == TrustSafetyActionItem::RiskTier::Risky);
    QVERIFY(items.at(0).description.contains("saved passwords"));
    QVERIFY(items.at(1).riskTier == TrustSafetyActionItem::RiskTier::Standard);
}

QTEST_MAIN(TestCleanerActionInterpreter)
#include "test_cleaner_action_interpreter.moc"
