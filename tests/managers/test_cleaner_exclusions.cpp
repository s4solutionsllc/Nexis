#include <QTest>
#include <QTemporaryDir>
#include <QFile>
#include <QDir>
#include <QStandardPaths>
#include "Managers/cleaner_service.h"
#include "Managers/setting_manager.h"

class TestCleanerExclusions : public QObject
{
    Q_OBJECT

private:
    using Entry = CleanerService::ExclusionEntry;

    QList<Entry> makeList(std::initializer_list<std::pair<Entry::Type, QString>> items)
    {
        QList<Entry> result;
        for (const auto &p : items) {
            Entry e;
            e.type = p.first;
            e.path = p.second;
            result.append(e);
        }
        return result;
    }

private slots:
    // SSO-3399: redirect QSettings / QStandardPaths to a sandboxed test
    // location so jsonRoundTrip / addDuplicate / removeEntry don't trample
    // the developer's real settings.ini (this suite used to overwrite
    // ~/.config/<org>/<app>/settings.ini in place every CI run).
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
    }

    void fileExact_match();
    void fileExact_noMatch();
    void folderExact_match();
    void folderChild_match();
    void folderSiblingPrefix_noMatch();
    void multipleEntries_anyMatch();
    void emptyList_nothingExcluded();
    void jsonRoundTrip();
    void addDuplicate_ignored();
    void removeEntry();
    void symlink_resolved();
};

void TestCleanerExclusions::fileExact_match()
{
    auto excl = makeList({{Entry::File, "/var/log/proftpd/controls.log"}});
    QVERIFY(CleanerService::isExcluded("/var/log/proftpd/controls.log", excl));
}

void TestCleanerExclusions::fileExact_noMatch()
{
    auto excl = makeList({{Entry::File, "/var/log/proftpd/controls.log"}});
    QVERIFY(!CleanerService::isExcluded("/var/log/proftpd/xferlog", excl));
}

void TestCleanerExclusions::folderExact_match()
{
    auto excl = makeList({{Entry::Folder, "/var/log/apache2"}});
    QVERIFY(CleanerService::isExcluded("/var/log/apache2", excl));
}

void TestCleanerExclusions::folderChild_match()
{
    auto excl = makeList({{Entry::Folder, "/var/log/apache2"}});
    QVERIFY(CleanerService::isExcluded("/var/log/apache2/access.log", excl));
    QVERIFY(CleanerService::isExcluded("/var/log/apache2/error.log", excl));
    QVERIFY(CleanerService::isExcluded("/var/log/apache2/subdir/deep.log", excl));
}

void TestCleanerExclusions::folderSiblingPrefix_noMatch()
{
    auto excl = makeList({{Entry::Folder, "/var/log"}});
    QVERIFY(!CleanerService::isExcluded("/var/logging/test.log", excl));
    QVERIFY(!CleanerService::isExcluded("/var/log2/test.log", excl));
}

void TestCleanerExclusions::multipleEntries_anyMatch()
{
    auto excl = makeList({
        {Entry::File,   "/tmp/keep.txt"},
        {Entry::Folder, "/var/log/mysql"}
    });
    QVERIFY(CleanerService::isExcluded("/tmp/keep.txt", excl));
    QVERIFY(CleanerService::isExcluded("/var/log/mysql/error.log", excl));
    QVERIFY(!CleanerService::isExcluded("/tmp/delete.txt", excl));
}

void TestCleanerExclusions::emptyList_nothingExcluded()
{
    QList<Entry> excl;
    QVERIFY(!CleanerService::isExcluded("/var/log/anything.log", excl));
    QVERIFY(!CleanerService::isExcluded("/home/user/.cache/something", excl));
}

void TestCleanerExclusions::jsonRoundTrip()
{
    auto original = makeList({
        {Entry::File,   "/var/log/proftpd/controls.log"},
        {Entry::Folder, "/var/log/apache2"},
        {Entry::Folder, "/home/user/.cache/JetBrains"}
    });

    CleanerService *cs = CleanerService::ins();
    cs->saveExclusions(original);
    auto loaded = cs->loadExclusions();

    QCOMPARE(loaded.size(), original.size());
    for (int i = 0; i < original.size(); ++i) {
        QCOMPARE(loaded[i].type, original[i].type);
        QCOMPARE(loaded[i].path, original[i].path);
    }
}

void TestCleanerExclusions::addDuplicate_ignored()
{
    CleanerService *cs = CleanerService::ins();
    cs->saveExclusions({});

    cs->addExclusion(Entry::File, "/tmp/test.log");
    cs->addExclusion(Entry::File, "/tmp/test.log");

    auto loaded = cs->loadExclusions();
    QCOMPARE(loaded.size(), 1);
}

void TestCleanerExclusions::removeEntry()
{
    CleanerService *cs = CleanerService::ins();
    auto entries = makeList({
        {Entry::File,   "/tmp/a.log"},
        {Entry::Folder, "/tmp/b"}
    });
    cs->saveExclusions(entries);

    cs->removeExclusion("/tmp/a.log");
    auto loaded = cs->loadExclusions();
    QCOMPARE(loaded.size(), 1);
    QCOMPARE(loaded[0].path, QString("/tmp/b"));
}

void TestCleanerExclusions::symlink_resolved()
{
    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());

    QString realFile = tmpDir.path() + "/real.log";
    QString linkFile = tmpDir.path() + "/link.log";

    QFile f(realFile);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("test");
    f.close();

    QVERIFY(QFile::link(realFile, linkFile));

    auto excl = makeList({{Entry::File, realFile}});
    QVERIFY(CleanerService::isExcluded(linkFile, excl));
}

QTEST_MAIN(TestCleanerExclusions)
#include "test_cleaner_exclusions.moc"
