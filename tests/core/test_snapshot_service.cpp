#include <QTest>

#include "Services/snapshot_service.h"

using Backend = SnapshotService::Backend;
using Probe = SnapshotService::Probe;

class TestSnapshotService : public QObject
{
    Q_OBJECT

private slots:
    void chooseLinuxBackend_nothingInstalled_none();
    void chooseLinuxBackend_timeshiftWinsOverOthers();
    void chooseLinuxBackend_snapperNeedsRootConfig();
    void chooseLinuxBackend_snapperUsedWhenConfigured();
    void chooseLinuxBackend_btrfsNeedsBtrfsRootFs();
    void chooseLinuxBackend_btrfsOnBtrfsRoot();
    void buildCommand_timeshift();
    void buildCommand_snapper();
    void buildCommand_btrfsKeepsPathsOutOfShellScript();
    void buildCommand_none_isEmpty();
    void backendDisplayName_perBackend();
};

void TestSnapshotService::chooseLinuxBackend_nothingInstalled_none()
{
    QCOMPARE(SnapshotService::chooseLinuxBackend(Probe{}), Backend::None);
}

void TestSnapshotService::chooseLinuxBackend_timeshiftWinsOverOthers()
{
    Probe p;
    p.timeshift = true;
    p.snapper = true;
    p.snapperRootConfig = true;
    p.btrfs = true;
    p.rootFsType = "btrfs";
    QCOMPARE(SnapshotService::chooseLinuxBackend(p), Backend::Timeshift);
}

void TestSnapshotService::chooseLinuxBackend_snapperNeedsRootConfig()
{
    Probe p;
    p.snapper = true;
    QCOMPARE(SnapshotService::chooseLinuxBackend(p), Backend::None);
}

void TestSnapshotService::chooseLinuxBackend_snapperUsedWhenConfigured()
{
    Probe p;
    p.snapper = true;
    p.snapperRootConfig = true;
    p.btrfs = true;
    p.rootFsType = "btrfs";
    QCOMPARE(SnapshotService::chooseLinuxBackend(p), Backend::Snapper);
}

void TestSnapshotService::chooseLinuxBackend_btrfsNeedsBtrfsRootFs()
{
    Probe p;
    p.btrfs = true;
    p.rootFsType = "ext4";
    QCOMPARE(SnapshotService::chooseLinuxBackend(p), Backend::None);
}

void TestSnapshotService::chooseLinuxBackend_btrfsOnBtrfsRoot()
{
    Probe p;
    p.btrfs = true;
    p.rootFsType = "btrfs";
    QCOMPARE(SnapshotService::chooseLinuxBackend(p), Backend::Btrfs);
}

void TestSnapshotService::buildCommand_timeshift()
{
    const auto cmd = SnapshotService::buildCommand(Backend::Timeshift, "Nexis pre-clean: Cache", "20261119-120000");
    QCOMPARE(cmd.program, QStringLiteral("timeshift"));
    QCOMPARE(cmd.args, (QStringList{"--create", "--comments", "Nexis pre-clean: Cache"}));
}

void TestSnapshotService::buildCommand_snapper()
{
    const auto cmd = SnapshotService::buildCommand(Backend::Snapper, "Nexis pre-clean: Cache", "20261119-120000");
    QCOMPARE(cmd.program, QStringLiteral("snapper"));
    QCOMPARE(cmd.args.mid(0, 3), (QStringList{"-c", "root", "create"}));
    QCOMPARE(cmd.args.last(), QStringLiteral("Nexis pre-clean: Cache"));
}

void TestSnapshotService::buildCommand_btrfsKeepsPathsOutOfShellScript()
{
    const auto cmd = SnapshotService::buildCommand(Backend::Btrfs, "ignored", "20261119-120000");
    QCOMPARE(cmd.program, QStringLiteral("sh"));
    QCOMPARE(cmd.args.value(0), QStringLiteral("-c"));
    QVERIFY(cmd.args.value(1).contains("btrfs subvolume snapshot -r /"));
    QVERIFY(!cmd.args.value(1).contains(SnapshotService::btrfsSnapshotDir()));
    QCOMPARE(cmd.args.value(3), SnapshotService::btrfsSnapshotDir());
    QCOMPARE(cmd.args.value(4), QStringLiteral("nexis-pre-clean-20261119-120000"));
}

void TestSnapshotService::buildCommand_none_isEmpty()
{
    const auto cmd = SnapshotService::buildCommand(Backend::None, "x", "y");
    QVERIFY(cmd.program.isEmpty());
    QVERIFY(cmd.args.isEmpty());
}

void TestSnapshotService::backendDisplayName_perBackend()
{
    QVERIFY(SnapshotService::backendDisplayName(Backend::None).isEmpty());
    QCOMPARE(SnapshotService::backendDisplayName(Backend::Timeshift), QStringLiteral("Timeshift"));
    QCOMPARE(SnapshotService::backendDisplayName(Backend::Snapper), QStringLiteral("snapper"));
    QCOMPARE(SnapshotService::backendDisplayName(Backend::Btrfs), QStringLiteral("btrfs snapshot"));
}

QTEST_MAIN(TestSnapshotService)
#include "test_snapshot_service.moc"
