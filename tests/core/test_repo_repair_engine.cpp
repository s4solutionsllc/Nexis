#include <QTest>
#include <QTemporaryDir>
#include <QFile>
#include <QDir>
#include "Tools/repo_health_types.h"
#include "Tools/repo_repair_engine.h"
#include "Tools/apt_source_tool.h"

#ifdef Q_OS_LINUX
#include "Tools/repo_repair_engine_linux.h"
#endif

// Test subclass that bypasses pkexec — writes directly
class TestableRepairEngine : public
#ifdef Q_OS_LINUX
    RepoRepairEngineLinux
#else
    RepoRepairEngine
#endif
{
protected:
    bool writeFileElevated(const QString &tempPath, const QString &destPath) override {
        QFile::remove(destPath);
        return QFile::copy(tempPath, destPath);
    }
    bool removeFileElevated(const QString &path) override {
        return QFile::remove(path);
    }
#ifndef Q_OS_LINUX
    RepairResult convertToDeb822(const APTSourcePtr &) override { return {false, {}, {}}; }
    RepairResult removeDuplicate(const APTSourcePtr &) override { return {false, {}, {}}; }
    RepairResult disableSource(const APTSourcePtr &) override { return {false, {}, {}}; }
    RepairResult enableSource(const APTSourcePtr &) override { return {false, {}, {}}; }
    RepairResult removeSource(const APTSourcePtr &) override { return {false, {}, {}}; }
    void diagnoseConnection(const APTSourcePtr &) override {}
#endif
};

class TestRepoRepairEngine : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir mTempDir;
    TestableRepairEngine mEngine;

    QString writeTestFile(const QString &name, const QString &content) {
        QString path = mTempDir.path() + "/" + name;
        QFile f(path);
        f.open(QIODevice::WriteOnly);
        f.write(content.toUtf8());
        f.close();
        return path;
    }

    QString readFile(const QString &path) {
        QFile f(path);
        f.open(QIODevice::ReadOnly);
        return QString::fromUtf8(f.readAll());
    }

private slots:
    void initTestCase();

#ifdef Q_OS_LINUX
    void disableSource_legacyList_commentsLine();
    void disableSource_deb822_addsEnabledNo();
    void enableSource_legacyList_uncommentsLine();
    void enableSource_deb822_removesEnabledNo();
    void removeSource_activeSource_refuses();
    void removeSource_disabledLegacy_removesLine();
    void removeSource_onlyEntryInFile_deletesFile();
    void removeDuplicate_commentsSecondOccurrence();
#endif
};

void TestRepoRepairEngine::initTestCase()
{
    qRegisterMetaType<DiagnoseResult>("DiagnoseResult");
}

#ifdef Q_OS_LINUX
void TestRepoRepairEngine::disableSource_legacyList_commentsLine()
{
    QString content = "deb http://archive.ubuntu.com/ubuntu jammy main restricted\n"
                      "deb http://other.example.com/repo stable main\n";
    QString path = writeTestFile("test.list", content);

    APTSourcePtr src(new APTSource);
    src->uri = "http://archive.ubuntu.com/ubuntu";
    src->suites = "jammy";
    src->components = "main restricted";
    src->filePath = path;
    src->isActive = true;
    src->format = APTSource::Legacy;

    auto result = mEngine.disableSource(src);
    QVERIFY(result.success);

    QString modified = readFile(path);
    QVERIFY(modified.contains("# deb http://archive.ubuntu.com/ubuntu jammy main restricted"));
    QVERIFY(modified.contains("deb http://other.example.com/repo stable main"));
}

void TestRepoRepairEngine::disableSource_deb822_addsEnabledNo()
{
    QString content = "Types: deb\n"
                      "URIs: http://example.com/repo\n"
                      "Suites: stable\n"
                      "Components: main\n";
    QString path = writeTestFile("test.sources", content);

    APTSourcePtr src(new APTSource);
    src->uri = "http://example.com/repo";
    src->filePath = path;
    src->isActive = true;
    src->format = APTSource::Deb822;

    auto result = mEngine.disableSource(src);
    QVERIFY(result.success);

    QString modified = readFile(path);
    QVERIFY(modified.contains("Enabled: no"));
}

void TestRepoRepairEngine::enableSource_legacyList_uncommentsLine()
{
    QString content = "# deb http://archive.ubuntu.com/ubuntu jammy main restricted\n";
    QString path = writeTestFile("test2.list", content);

    APTSourcePtr src(new APTSource);
    src->uri = "http://archive.ubuntu.com/ubuntu";
    src->suites = "jammy";
    src->components = "main restricted";
    src->filePath = path;
    src->isActive = false;
    src->format = APTSource::Legacy;

    auto result = mEngine.enableSource(src);
    QVERIFY(result.success);

    QString modified = readFile(path);
    QVERIFY(modified.contains("deb http://archive.ubuntu.com/ubuntu"));
    QVERIFY(!modified.contains("# deb"));
}

void TestRepoRepairEngine::enableSource_deb822_removesEnabledNo()
{
    QString content = "Types: deb\n"
                      "URIs: http://example.com/repo\n"
                      "Suites: stable\n"
                      "Components: main\n"
                      "Enabled: no\n";
    QString path = writeTestFile("test2.sources", content);

    APTSourcePtr src(new APTSource);
    src->uri = "http://example.com/repo";
    src->filePath = path;
    src->isActive = false;
    src->format = APTSource::Deb822;

    auto result = mEngine.enableSource(src);
    QVERIFY(result.success);

    QString modified = readFile(path);
    QVERIFY(!modified.contains("Enabled: no"));
    QVERIFY(modified.contains("URIs: http://example.com/repo"));
}

void TestRepoRepairEngine::removeSource_activeSource_refuses()
{
    APTSourcePtr src(new APTSource);
    src->isActive = true;

    auto result = mEngine.removeSource(src);
    QVERIFY(!result.success);
}

void TestRepoRepairEngine::removeSource_disabledLegacy_removesLine()
{
    QString content = "# deb http://old.repo.com/ubuntu jammy main\n"
                      "deb http://other.repo.com/ubuntu jammy main\n";
    QString path = writeTestFile("test3.list", content);

    APTSourcePtr src(new APTSource);
    src->uri = "http://old.repo.com/ubuntu";
    src->suites = "jammy";
    src->components = "main";
    src->filePath = path;
    src->isActive = false;
    src->format = APTSource::Legacy;

    auto result = mEngine.removeSource(src);
    QVERIFY(result.success);

    QString modified = readFile(path);
    QVERIFY(!modified.contains("old.repo.com"));
    QVERIFY(modified.contains("other.repo.com"));
}

void TestRepoRepairEngine::removeSource_onlyEntryInFile_deletesFile()
{
    QString content = "# deb http://dead.repo.com/ubuntu jammy main\n";
    QString path = writeTestFile("single.list", content);

    APTSourcePtr src(new APTSource);
    src->uri = "http://dead.repo.com/ubuntu";
    src->suites = "jammy";
    src->components = "main";
    src->filePath = path;
    src->isActive = false;
    src->format = APTSource::Legacy;

    auto result = mEngine.removeSource(src);
    QVERIFY(result.success);
    QVERIFY(!QFile::exists(path));
}
void TestRepoRepairEngine::removeDuplicate_commentsSecondOccurrence()
{
    QString content = "deb http://repo.example.com/ubuntu jammy main\n"
                      "deb http://other.example.com/ubuntu jammy main\n"
                      "deb http://repo.example.com/ubuntu jammy main\n";
    QString path = writeTestFile("dupes.list", content);

    APTSourcePtr src(new APTSource);
    src->uri = "http://repo.example.com/ubuntu";
    src->suites = "jammy";
    src->components = "main";
    src->filePath = path;
    src->isActive = true;
    src->format = APTSource::Legacy;

    auto result = mEngine.removeDuplicate(src);
    QVERIFY(result.success);

    QString modified = readFile(path);
    int commentedCount = modified.count("# Disabled by Nexis: duplicate entry");
    QCOMPARE(commentedCount, 1);
    // First occurrence should still be active
    QVERIFY(modified.startsWith("deb http://repo.example.com/ubuntu jammy main\n"));
}
#endif

QTEST_MAIN(TestRepoRepairEngine)
#include "test_repo_repair_engine.moc"
