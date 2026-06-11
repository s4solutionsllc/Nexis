#include <QTest>
#include "Tools/upgrade_command.h"

// SSO-3741 (FW-13): the upgrade-command builder converts an UpdateEntry into
// the exact argv that CommandUtil::{sudo,}execWithStatus runs. The whole point
// of having it as a pure helper is that we can test the construction without
// shelling out — we assert the program + args + sudo flag here.
class TestUpgradeCommand : public QObject
{
    Q_OBJECT

private slots:
    void build_apt_single_constructsApt();
    void build_dnf_single_constructsDnf();
    void build_pacman_single_constructsPacman();
    void build_zypper_single_constructsZypper();
    void build_snap_single_constructsSnap();
    void build_flatpak_single_doesNotElevate();
    void build_brew_single_doesNotElevate();
    void build_system_single_constructsSoftwareupdate();

    void buildAll_apt_constructsDistUpgrade();
    void buildAll_dnf_constructsUpgradeAll();
    void buildAll_pacman_constructsSyu();
    void buildAll_zypper_constructsUpdateAll();
    void buildAll_snap_constructsRefreshAll();
    void buildAll_flatpak_constructsUpdateAll();
    void buildAll_brew_constructsUpgradeAll();
    void buildAll_system_constructsInstallAll();

    void build_unknownSource_returnsInvalid();
    void build_emptyName_returnsInvalid();
};

static UpdateEntry mkEntry(const QString &source, const QString &name)
{
    UpdateEntry e;
    e.source = source;
    e.name = name;
    return e;
}

void TestUpgradeCommand::build_apt_single_constructsApt()
{
    auto cmd = UpgradeCommandBuilder::build(mkEntry("apt", "curl"));
    QVERIFY(cmd.valid);
    QCOMPARE(cmd.program, QString("apt-get"));
    QCOMPARE(cmd.args, (QStringList{"-y", "install", "--only-upgrade", "curl"}));
    QVERIFY(cmd.requiresSudo);
}

void TestUpgradeCommand::build_dnf_single_constructsDnf()
{
    auto cmd = UpgradeCommandBuilder::build(mkEntry("dnf", "kernel"));
    QVERIFY(cmd.valid);
    QCOMPARE(cmd.program, QString("dnf"));
    QCOMPARE(cmd.args, (QStringList{"-y", "upgrade", "kernel"}));
    QVERIFY(cmd.requiresSudo);
}

void TestUpgradeCommand::build_pacman_single_constructsPacman()
{
    auto cmd = UpgradeCommandBuilder::build(mkEntry("pacman", "linux"));
    QVERIFY(cmd.valid);
    QCOMPARE(cmd.program, QString("pacman"));
    QCOMPARE(cmd.args, (QStringList{"-S", "--noconfirm", "linux"}));
    QVERIFY(cmd.requiresSudo);
}

void TestUpgradeCommand::build_zypper_single_constructsZypper()
{
    auto cmd = UpgradeCommandBuilder::build(mkEntry("zypper", "bash"));
    QVERIFY(cmd.valid);
    QCOMPARE(cmd.program, QString("zypper"));
    QCOMPARE(cmd.args, (QStringList{"--non-interactive", "update", "bash"}));
    QVERIFY(cmd.requiresSudo);
}

void TestUpgradeCommand::build_snap_single_constructsSnap()
{
    auto cmd = UpgradeCommandBuilder::build(mkEntry("snap", "firefox"));
    QVERIFY(cmd.valid);
    QCOMPARE(cmd.program, QString("snap"));
    QCOMPARE(cmd.args, (QStringList{"refresh", "firefox"}));
    QVERIFY(cmd.requiresSudo);
}

void TestUpgradeCommand::build_flatpak_single_doesNotElevate()
{
    auto cmd = UpgradeCommandBuilder::build(mkEntry("flatpak", "org.mozilla.firefox"));
    QVERIFY(cmd.valid);
    QCOMPARE(cmd.program, QString("flatpak"));
    QCOMPARE(cmd.args, (QStringList{"update", "-y", "org.mozilla.firefox"}));
    QVERIFY(!cmd.requiresSudo);  // flatpak elevates itself for system-scope installs
}

void TestUpgradeCommand::build_brew_single_doesNotElevate()
{
    auto cmd = UpgradeCommandBuilder::build(mkEntry("brew", "wget"));
    QVERIFY(cmd.valid);
    QCOMPARE(cmd.program, QString("brew"));
    QCOMPARE(cmd.args, (QStringList{"upgrade", "wget"}));
    QVERIFY(!cmd.requiresSudo);  // Homebrew refuses to run as root.
}

void TestUpgradeCommand::build_system_single_constructsSoftwareupdate()
{
    auto cmd = UpgradeCommandBuilder::build(mkEntry("system", "macOS Sonoma 14.5-23F79"));
    QVERIFY(cmd.valid);
    QCOMPARE(cmd.program, QString("softwareupdate"));
    QCOMPARE(cmd.args, (QStringList{"-i", "macOS Sonoma 14.5-23F79"}));
    QVERIFY(cmd.requiresSudo);
}

void TestUpgradeCommand::buildAll_apt_constructsDistUpgrade()
{
    auto cmd = UpgradeCommandBuilder::buildAll("apt");
    QVERIFY(cmd.valid);
    QCOMPARE(cmd.program, QString("apt-get"));
    QCOMPARE(cmd.args, (QStringList{"-y", "dist-upgrade"}));
}

void TestUpgradeCommand::buildAll_dnf_constructsUpgradeAll()
{
    auto cmd = UpgradeCommandBuilder::buildAll("dnf");
    QCOMPARE(cmd.args, (QStringList{"-y", "upgrade"}));
}

void TestUpgradeCommand::buildAll_pacman_constructsSyu()
{
    auto cmd = UpgradeCommandBuilder::buildAll("pacman");
    QCOMPARE(cmd.args, (QStringList{"-Syu", "--noconfirm"}));
}

void TestUpgradeCommand::buildAll_zypper_constructsUpdateAll()
{
    auto cmd = UpgradeCommandBuilder::buildAll("zypper");
    QCOMPARE(cmd.args, (QStringList{"--non-interactive", "update"}));
}

void TestUpgradeCommand::buildAll_snap_constructsRefreshAll()
{
    auto cmd = UpgradeCommandBuilder::buildAll("snap");
    QCOMPARE(cmd.args, (QStringList{"refresh"}));
}

void TestUpgradeCommand::buildAll_flatpak_constructsUpdateAll()
{
    auto cmd = UpgradeCommandBuilder::buildAll("flatpak");
    QCOMPARE(cmd.args, (QStringList{"update", "-y"}));
    QVERIFY(!cmd.requiresSudo);
}

void TestUpgradeCommand::buildAll_brew_constructsUpgradeAll()
{
    auto cmd = UpgradeCommandBuilder::buildAll("brew");
    QCOMPARE(cmd.args, (QStringList{"upgrade"}));
    QVERIFY(!cmd.requiresSudo);
}

void TestUpgradeCommand::buildAll_system_constructsInstallAll()
{
    auto cmd = UpgradeCommandBuilder::buildAll("system");
    QCOMPARE(cmd.program, QString("softwareupdate"));
    QCOMPARE(cmd.args, (QStringList{"-ia"}));
    QVERIFY(cmd.requiresSudo);
}

void TestUpgradeCommand::build_unknownSource_returnsInvalid()
{
    auto cmd = UpgradeCommandBuilder::build(mkEntry("nuget", "foo"));
    QVERIFY(!cmd.valid);
}

void TestUpgradeCommand::build_emptyName_returnsInvalid()
{
    auto cmd = UpgradeCommandBuilder::build(mkEntry("apt", ""));
    QVERIFY(!cmd.valid);
}

QTEST_MAIN(TestUpgradeCommand)
#include "test_upgrade_command.moc"
