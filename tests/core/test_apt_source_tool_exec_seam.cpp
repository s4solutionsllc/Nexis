// SSO-3470 (WI-05.b): AptSourceToolLinux writes/removes repo files through
// direct CommandUtil::sudoExec calls (no runSudoCommand seam), now migrated
// to sudoExecWithStatus branching on ExecResult::ok(). test_apt_source_tool.cpp
// only exercises the pure parse/serialize helpers; these tests drive the real
// changeSource() write paths ("tee" / "rm") end-to-end against a temp file,
// using the NEXIS_SUDO_BYPASS=1 seam (see tests/utils/test_command_util.cpp)
// so no pkexec prompt is needed.

#include <QTest>
#include <QTemporaryDir>
#include <QFile>
#include "apt_source_tool_linux.h"

class TestAptSourceToolExecSeam : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    void changeSource_deb822_statusToggle_rewritesFileViaTee();
    void changeSource_deb822_removeOnlyStanza_deletesFileViaRm();
    void changeSource_legacyList_removeEntry_rewritesFileViaTee();
};

void TestAptSourceToolExecSeam::initTestCase()
{
    qputenv("NEXIS_SUDO_BYPASS", "1");
}

void TestAptSourceToolExecSeam::cleanupTestCase()
{
    qunsetenv("NEXIS_SUDO_BYPASS");
}

static QString writeTempFile(QTemporaryDir &dir, const QString &name, const QString &content)
{
    const QString path = dir.filePath(name);
    QFile f(path);
    const bool opened = f.open(QIODevice::WriteOnly | QIODevice::Text);
    Q_ASSERT(opened);
    f.write(content.toUtf8());
    f.close();
    return path;
}

void TestAptSourceToolExecSeam::changeSource_deb822_statusToggle_rewritesFileViaTee()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString stanza =
        "Types: deb\n"
        "URIs: http://example.com/apt\n"
        "Suites: noble\n"
        "Components: main\n";
    const QString path = writeTempFile(dir, "test.sources", stanza);

    auto parsed = AptSourceTool::parseDeb822Stanza(stanza, "deb", "deb-src");
    QCOMPARE(parsed.size(), 1);
    APTSourcePtr src = parsed.first();
    src->filePath = path;

    APTSourcePtr updated(new APTSource(*src));
    updated->isActive = false; // disable this source

    AptSourceToolLinux tool;
    tool.changeSource(src, updated);

    QFile f(path);
    QVERIFY(f.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString newContent = QString::fromUtf8(f.readAll());
    QVERIFY2(newContent.contains("Enabled: no"),
              qPrintable(QString("tee did not rewrite file as expected:\n%1").arg(newContent)));
}

void TestAptSourceToolExecSeam::changeSource_deb822_removeOnlyStanza_deletesFileViaRm()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString stanza =
        "Types: deb\n"
        "URIs: http://example.com/apt\n"
        "Suites: noble\n"
        "Components: main\n";
    const QString path = writeTempFile(dir, "test2.sources", stanza);

    auto parsed = AptSourceTool::parseDeb822Stanza(stanza, "deb", "deb-src");
    QCOMPARE(parsed.size(), 1);
    APTSourcePtr src = parsed.first();
    src->filePath = path;

    AptSourceToolLinux tool;
    tool.changeSource(src, APTSourcePtr()); // remove the only stanza -> empty content -> rm

    QVERIFY(!QFile::exists(path));
}

void TestAptSourceToolExecSeam::changeSource_legacyList_removeEntry_rewritesFileViaTee()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString listContent =
        "deb http://archive.ubuntu.com/ubuntu jammy main\n"
        "deb http://example.com/apt stable main\n";
    const QString path = writeTempFile(dir, "test.list", listContent);

    APTSourcePtr src = AptSourceTool::parseSourceListLine(
        "deb http://example.com/apt stable main", "deb", "deb-src");
    QVERIFY(src);
    src->filePath = path;

    AptSourceToolLinux tool;
    tool.changeSource(src, APTSourcePtr()); // remove just this line

    QFile f(path);
    QVERIFY(f.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString newContent = QString::fromUtf8(f.readAll());
    QVERIFY(!newContent.contains("example.com"));
    QVERIFY(newContent.contains("archive.ubuntu.com"));
}

QTEST_MAIN(TestAptSourceToolExecSeam)
#include "test_apt_source_tool_exec_seam.moc"
