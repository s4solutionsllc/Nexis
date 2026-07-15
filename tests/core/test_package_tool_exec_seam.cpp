// SSO-3470 (WI-05.b): PackageTool::runCommand / runSudoCommand are the
// migrated seam in shared/nexis-core/Tools/package_tool_shared.cpp — they now
// delegate to execWithStatus/sudoExecWithStatus and branch on ExecResult::ok()
// instead of a try/catch(const QString&) around the legacy throwing exec().
// test_package_tool_uninstall.cpp exercises the seam through an override that
// only captures (cmd, args); these tests exercise the REAL default
// implementation end-to-end against real processes, using the
// NEXIS_SUDO_BYPASS=1 seam (see tests/utils/test_command_util.cpp) so
// runSudoCommand doesn't try to pop a pkexec prompt.

#include <QTest>
#include "package_tool_linux.h"

namespace {

// runCommand/runSudoCommand are protected on PackageTool so production
// subclasses can only reach them internally; re-expose for the test.
class ExposedPackageToolLinux : public PackageToolLinux
{
public:
    using PackageTool::runCommand;
    using PackageTool::runSudoCommand;
};

} // namespace

class TestPackageToolExecSeam : public QObject
{
    Q_OBJECT

private slots:
    void runCommand_success_returnsTrimmedOutput();
    void runCommand_failure_returnsEmptyStringDoesNotThrow();
    void runSudoCommand_bypass_success_returnsTrue();
    void runSudoCommand_bypass_failure_returnsFalse();
};

void TestPackageToolExecSeam::runCommand_success_returnsTrimmedOutput()
{
    ExposedPackageToolLinux tool;
    QString out = tool.runCommand("echo", {"hello-seam"});
    QCOMPARE(out.trimmed(), QString("hello-seam"));
}

void TestPackageToolExecSeam::runCommand_failure_returnsEmptyStringDoesNotThrow()
{
    ExposedPackageToolLinux tool;
    bool threw = false;
    QString out;
    try {
        out = tool.runCommand("nonexistent_binary_xyz_12345", {});
    } catch (...) {
        threw = true;
    }
    QVERIFY2(!threw, "PackageTool::runCommand must not throw (SSO-3470 execWithStatus migration)");
    QVERIFY(out.isEmpty());
}

void TestPackageToolExecSeam::runSudoCommand_bypass_success_returnsTrue()
{
    qputenv("NEXIS_SUDO_BYPASS", "1");
    ExposedPackageToolLinux tool;
    bool ok = tool.runSudoCommand("echo", {"sudo-seam"});
    qunsetenv("NEXIS_SUDO_BYPASS");
    QVERIFY(ok);
}

void TestPackageToolExecSeam::runSudoCommand_bypass_failure_returnsFalse()
{
    qputenv("NEXIS_SUDO_BYPASS", "1");
    ExposedPackageToolLinux tool;
    bool ok = tool.runSudoCommand("false", {});
    qunsetenv("NEXIS_SUDO_BYPASS");
    QVERIFY(!ok);
}

QTEST_MAIN(TestPackageToolExecSeam)
#include "test_package_tool_exec_seam.moc"
