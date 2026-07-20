// SSO-15379: smoke test for the eBPF per-process network loader. This links
// nexis-core (built with the same NEXIS_HAVE_EBPF state — see CMakeLists.txt,
// where that define is PUBLIC precisely so this test sees the same class
// layout) rather than compiling net_acct_bpf_loader.cpp directly.
//
// This environment's CI container is very unlikely to grant CAP_BPF to an
// unprivileged build user, and may not even have libbpf/a BPF-target clang
// installed at all — so this test intentionally does not assert
// Status::Loaded. What it does assert: the loader never crashes, always
// reaches a terminal status, and always explains itself when it isn't
// working. Getting to Status::Loaded (or even just past PermissionDenied)
// can only be confirmed on a machine with CAP_BPF/root and a kernel new
// enough for the two kprobes — flagged here rather than silently assumed.

#include <QTest>

#include "net_acct_bpf_loader.h"

class TestNetAcctBpfLoader : public QObject
{
    Q_OBJECT

private slots:
    void ensureLoaded_reachesTerminalStatusWithoutCrashing();
    void ensureLoaded_isIdempotent();
    void lookup_beforeLoad_returnsFalse();
};

void TestNetAcctBpfLoader::ensureLoaded_reachesTerminalStatusWithoutCrashing()
{
    NetAcctBpfLoader loader;
    QCOMPARE(loader.status(), NetAcctBpfLoader::Status::NotLoaded);

    loader.ensureLoaded();

    QVERIFY(loader.status() == NetAcctBpfLoader::Status::Loaded
             || loader.status() == NetAcctBpfLoader::Status::PermissionDenied
             || loader.status() == NetAcctBpfLoader::Status::Unavailable);

    if (loader.status() != NetAcctBpfLoader::Status::Loaded)
        QVERIFY(!loader.lastError().isEmpty());
}

void TestNetAcctBpfLoader::ensureLoaded_isIdempotent()
{
    NetAcctBpfLoader loader;
    loader.ensureLoaded();
    const NetAcctBpfLoader::Status first = loader.status();
    loader.ensureLoaded();
    QCOMPARE(loader.status(), first);
}

void TestNetAcctBpfLoader::lookup_beforeLoad_returnsFalse()
{
    NetAcctBpfLoader loader;
    quint64 tx = 0, rx = 0;
    QVERIFY(!loader.lookup(1, &tx, &rx));
}

QTEST_MAIN(TestNetAcctBpfLoader)
#include "test_net_acct_bpf_loader.moc"
