// SSO-3745 (FW-17): command-construction tests for MacOSMaintenancePanel.
//
// Tests verify that defaultTasks() returns the expected commands and argument
// vectors for each maintenance task. No real commands are executed; the seam
// is the static defaultTasks() function which is pure data and compiles on
// any platform. QSKIP'd off-macOS as a belt-and-braces guard.

#include <QTest>
#include "Pages/Dashboard/macos_maintenance_panel.h"

class TestMacOSMaintenance : public QObject
{
    Q_OBJECT

private slots:
    void defaultTasks_returnsAtLeastFiveTasks();
    void defaultTasks_allTasksHaveNonEmptyFields();
    void spotlightReindex_correctArgv();
    void verifyDisk_correctArgv();
    void rebuildLaunchServices_correctArgv();
    void flushDns_correctArgv();
    void finderHiddenFiles_correctArgv();
    void finderShowPathBar_correctArgv();
    void finderShowStatusBar_correctArgv();
    void finderQuitMenu_correctArgv();
};

static MacOSMaintenanceTask taskById(const QString &id)
{
    for (const MacOSMaintenanceTask &t : MacOSMaintenancePanel::defaultTasks()) {
        if (t.id == id)
            return t;
    }
    return {};
}

void TestMacOSMaintenance::defaultTasks_returnsAtLeastFiveTasks()
{
#ifndef Q_OS_MACOS
    QSKIP("macOS-only test");
#endif
    QVERIFY(MacOSMaintenancePanel::defaultTasks().size() >= 5);
}

void TestMacOSMaintenance::defaultTasks_allTasksHaveNonEmptyFields()
{
#ifndef Q_OS_MACOS
    QSKIP("macOS-only test");
#endif
    for (const MacOSMaintenanceTask &t : MacOSMaintenancePanel::defaultTasks()) {
        QVERIFY2(!t.id.isEmpty(),          qPrintable("empty id"));
        QVERIFY2(!t.title.isEmpty(),       qPrintable("empty title for " + t.id));
        QVERIFY2(!t.description.isEmpty(), qPrintable("empty description for " + t.id));
        QVERIFY2(!t.cmd.isEmpty(),         qPrintable("empty cmd for " + t.id));
        QVERIFY2(t.timeoutMs > 0,          qPrintable("non-positive timeout for " + t.id));
    }
}

void TestMacOSMaintenance::spotlightReindex_correctArgv()
{
#ifndef Q_OS_MACOS
    QSKIP("macOS-only test");
#endif
    MacOSMaintenanceTask t = taskById("spotlight_reindex");
    QVERIFY(!t.id.isEmpty());
    QCOMPARE(t.cmd,  QString("mdutil"));
    QCOMPARE(t.args, QStringList({"-E", "/"}));
    QVERIFY(t.needsSudo);
    QVERIFY(t.cmd2.isEmpty());
}

void TestMacOSMaintenance::verifyDisk_correctArgv()
{
#ifndef Q_OS_MACOS
    QSKIP("macOS-only test");
#endif
    MacOSMaintenanceTask t = taskById("verify_disk");
    QVERIFY(!t.id.isEmpty());
    QCOMPARE(t.cmd,  QString("diskutil"));
    QCOMPARE(t.args, QStringList({"verifyVolume", "/"}));
    QVERIFY(!t.needsSudo);
}

void TestMacOSMaintenance::rebuildLaunchServices_correctArgv()
{
#ifndef Q_OS_MACOS
    QSKIP("macOS-only test");
#endif
    MacOSMaintenanceTask t = taskById("rebuild_launch_services");
    QVERIFY(!t.id.isEmpty());
    QVERIFY(t.cmd.contains("lsregister"));
    const QStringList expectedArgs = {"-r", "-domain", "local",
                                      "-domain", "system", "-domain", "user"};
    QCOMPARE(t.args, expectedArgs);
    QVERIFY(!t.needsSudo);
    QCOMPARE(t.cmd2,  QString("killall"));
    QCOMPARE(t.args2, QStringList({"Finder"}));
}

void TestMacOSMaintenance::flushDns_correctArgv()
{
#ifndef Q_OS_MACOS
    QSKIP("macOS-only test");
#endif
    MacOSMaintenanceTask t = taskById("flush_dns");
    QVERIFY(!t.id.isEmpty());
    QCOMPARE(t.cmd,  QString("dscacheutil"));
    QCOMPARE(t.args, QStringList({"-flushcache"}));
    QVERIFY(t.needsSudo);
    QCOMPARE(t.cmd2,  QString("killall"));
    QCOMPARE(t.args2, QStringList({"-HUP", "mDNSResponder"}));
}

void TestMacOSMaintenance::finderHiddenFiles_correctArgv()
{
#ifndef Q_OS_MACOS
    QSKIP("macOS-only test");
#endif
    MacOSMaintenanceTask t = taskById("finder_show_hidden");
    QVERIFY(!t.id.isEmpty());
    QCOMPARE(t.cmd,  QString("defaults"));
    QCOMPARE(t.args, QStringList({"write", "com.apple.finder",
                                  "AppleShowAllFiles", "-bool", "true"}));
    QVERIFY(!t.needsSudo);
    QCOMPARE(t.cmd2,  QString("killall"));
    QCOMPARE(t.args2, QStringList({"Finder"}));
}

void TestMacOSMaintenance::finderShowPathBar_correctArgv()
{
#ifndef Q_OS_MACOS
    QSKIP("macOS-only test");
#endif
    MacOSMaintenanceTask t = taskById("finder_show_path_bar");
    QCOMPARE(t.cmd,  QString("defaults"));
    QCOMPARE(t.args, QStringList({"write", "com.apple.finder",
                                  "ShowPathbar", "-bool", "true"}));
    QCOMPARE(t.cmd2,  QString("killall"));
    QCOMPARE(t.args2, QStringList({"Finder"}));
}

void TestMacOSMaintenance::finderShowStatusBar_correctArgv()
{
#ifndef Q_OS_MACOS
    QSKIP("macOS-only test");
#endif
    MacOSMaintenanceTask t = taskById("finder_show_status_bar");
    QCOMPARE(t.cmd,  QString("defaults"));
    QCOMPARE(t.args, QStringList({"write", "com.apple.finder",
                                  "ShowStatusBar", "-bool", "true"}));
    QCOMPARE(t.cmd2,  QString("killall"));
    QCOMPARE(t.args2, QStringList({"Finder"}));
}

void TestMacOSMaintenance::finderQuitMenu_correctArgv()
{
#ifndef Q_OS_MACOS
    QSKIP("macOS-only test");
#endif
    MacOSMaintenanceTask t = taskById("finder_quit_menu");
    QCOMPARE(t.cmd,  QString("defaults"));
    QCOMPARE(t.args, QStringList({"write", "com.apple.finder",
                                  "QuitMenuItem", "-bool", "true"}));
    QCOMPARE(t.cmd2,  QString("killall"));
    QCOMPARE(t.args2, QStringList({"Finder"}));
}

QTEST_MAIN(TestMacOSMaintenance)
#include "test_macos_maintenance.moc"
