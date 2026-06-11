// WI-33: command-construction tests for the platform PackageTool subclasses.
// Subclasses the production tools and overrides the runSudoCommand /
// runCommand seam (introduced on PackageTool for this purpose), then asserts
// the (cmd, args) tuple constructed for each uninstall path. Mirrors the
// TestableRepairEngine pattern in tests/core/test_repo_repair_engine.cpp.

#include <QStringList>
#include <QTest>

#include "Tools/package_tool_shared.h"
#include "Utils/command_util.h"
#include "package_tool_linux.h"
#include "package_tool_macos.h"

namespace {

struct ExecCall {
    enum Kind { Sudo, Plain };
    Kind kind;
    QString cmd;
    QStringList args;
    QString stubOutput;   // when Plain, returned to the caller
};

// ── Linux ──────────────────────────────────────────────────────────────────────

class CapturingPackageToolLinux : public PackageToolLinux
{
public:
    QList<ExecCall> calls;
    QString plainOutputForNextCall;   // returned by next runCommand call

protected:
    bool runSudoCommand(const QString &cmd, const QStringList &args) override
    {
        calls.push_back({ExecCall::Sudo, cmd, args, QString()});
        return true;
    }
    QString runCommand(const QString &cmd,
                       const QStringList &args,
                       int /*timeoutMs*/) override
    {
        ExecCall c{ExecCall::Plain, cmd, args, plainOutputForNextCall};
        calls.push_back(c);
        const QString out = plainOutputForNextCall;
        plainOutputForNextCall.clear();
        return out;
    }
};

// ── macOS ──────────────────────────────────────────────────────────────────────

class CapturingPackageToolMacOS : public PackageToolMacOS
{
public:
    QList<ExecCall> calls;

    void setBrewPath(const QString &p) { mBrew = p; }

protected:
    QString resolveBrewPath() const override { return mBrew; }

    bool runSudoCommand(const QString &cmd, const QStringList &args) override
    {
        calls.push_back({ExecCall::Sudo, cmd, args, QString()});
        return true;
    }
    QString runCommand(const QString &cmd,
                       const QStringList &args,
                       int /*timeoutMs*/) override
    {
        calls.push_back({ExecCall::Plain, cmd, args, QString()});
        return QString();
    }

private:
    QString mBrew = QStringLiteral("/opt/homebrew/bin/brew");
};

} // namespace

class TestPackageToolUninstall : public QObject
{
    Q_OBJECT

private slots:
    // ── PackageToolLinux::uninstallPackages ─────────────────────────────────
    void apt_uninstall_remove_passesNamesAndYesFlag();
    void apt_uninstall_purge_usesPurgeVerb();
    void apt_uninstall_preservesNamesWithMetacharacters();
    void dnf_uninstall_passesNamesAndYesFlag();
    void yum_uninstall_passesNamesAndYesFlag();
    void pacman_uninstall_appendsNoconfirmAndR();

    // ── PackageToolLinux::uninstallSnapPackages ─────────────────────────────
    void snap_uninstall_passesRemoveAndNames();

    // ── PackageToolLinux::removeOrphanPackages ──────────────────────────────
    void orphan_apt_callsAutoremoveYes();
    void orphan_dnf_callsAutoremoveYes();
    void orphan_yum_callsAutoremoveYes();
    void orphan_pacman_queriesThenRemovesEachOrphan();
    void orphan_pacman_emptyOutput_skipsRemove();

    // ── PackageToolLinux::removeStaleSnapRevisions ──────────────────────────
    void staleSnap_buildsRevisionFlagPerPackage();

    // ── PackageToolMacOS uninstall paths ────────────────────────────────────
    void brew_uninstall_passesUninstallVerb();
    void brew_uninstall_includesAllPackageNames();
    void brew_removeOrphan_callsAutoremove();

    // ── CommandUtil::buildMacOsSudoShellCommand (sudoExec escaping) ─────────
    void macSudo_quotesEachArgument();
    void macSudo_escapesEmbeddedSingleQuote();
    void macSudo_escapesBackslashAndDoubleQuote();
    void macSudo_preservesPackageNamesWithDashes();
    void macSudo_safelyHandlesShellInjectionAttempt();
};

// ── PackageToolLinux::uninstallPackages ───────────────────────────────────────

void TestPackageToolUninstall::apt_uninstall_remove_passesNamesAndYesFlag()
{
    CapturingPackageToolLinux tool;
    tool.currentPackageTool = APT;
    tool.uninstallPackages({"firefox", "libfoo-dev"}, /*purge=*/false);

    QCOMPARE(tool.calls.size(), 1);
    const auto &c = tool.calls.first();
    QCOMPARE(c.kind, ExecCall::Sudo);
    QCOMPARE(c.cmd, QStringLiteral("apt-get"));
    QCOMPARE(c.args, (QStringList{"remove", "-y", "firefox", "libfoo-dev"}));
}

void TestPackageToolUninstall::apt_uninstall_purge_usesPurgeVerb()
{
    CapturingPackageToolLinux tool;
    tool.currentPackageTool = APT;
    tool.uninstallPackages({"firefox"}, /*purge=*/true);

    QCOMPARE(tool.calls.size(), 1);
    QCOMPARE(tool.calls.first().args,
             (QStringList{"purge", "-y", "firefox"}));
}

void TestPackageToolUninstall::apt_uninstall_preservesNamesWithMetacharacters()
{
    // pkexec passes argv directly to apt-get without going through a shell,
    // so dangerous-looking strings just become literal package names.
    CapturingPackageToolLinux tool;
    tool.currentPackageTool = APT;
    tool.uninstallPackages({"pkg; rm -rf /", "lib bar"}, /*purge=*/false);

    QCOMPARE(tool.calls.size(), 1);
    QCOMPARE(tool.calls.first().args,
             (QStringList{"remove", "-y", "pkg; rm -rf /", "lib bar"}));
}

void TestPackageToolUninstall::dnf_uninstall_passesNamesAndYesFlag()
{
    CapturingPackageToolLinux tool;
    tool.currentPackageTool = DNF;
    tool.uninstallPackages({"firefox"}, /*purge=*/false);

    QCOMPARE(tool.calls.first().cmd, QStringLiteral("dnf"));
    QCOMPARE(tool.calls.first().args,
             (QStringList{"remove", "-y", "firefox"}));
}

void TestPackageToolUninstall::yum_uninstall_passesNamesAndYesFlag()
{
    CapturingPackageToolLinux tool;
    tool.currentPackageTool = YUM;
    tool.uninstallPackages({"httpd"}, /*purge=*/false);

    QCOMPARE(tool.calls.first().cmd, QStringLiteral("yum"));
    QCOMPARE(tool.calls.first().args,
             (QStringList{"remove", "-y", "httpd"}));
}

void TestPackageToolUninstall::pacman_uninstall_appendsNoconfirmAndR()
{
    CapturingPackageToolLinux tool;
    tool.currentPackageTool = PACMAN;
    tool.uninstallPackages({"firefox", "libfoo"}, /*purge=*/false);

    QCOMPARE(tool.calls.first().cmd, QStringLiteral("pacman"));
    // pacman appends --noconfirm and -R AFTER the package names.
    QCOMPARE(tool.calls.first().args,
             (QStringList{"firefox", "libfoo", "--noconfirm", "-R"}));
}

// ── PackageToolLinux::uninstallSnapPackages ───────────────────────────────────

void TestPackageToolUninstall::snap_uninstall_passesRemoveAndNames()
{
    CapturingPackageToolLinux tool;
    QVERIFY(tool.uninstallSnapPackages({"firefox", "core20"}));

    QCOMPARE(tool.calls.size(), 1);
    QCOMPARE(tool.calls.first().cmd, QStringLiteral("snap"));
    QCOMPARE(tool.calls.first().args,
             (QStringList{"remove", "firefox", "core20"}));
}

// ── PackageToolLinux::removeOrphanPackages ────────────────────────────────────

void TestPackageToolUninstall::orphan_apt_callsAutoremoveYes()
{
    CapturingPackageToolLinux tool;
    tool.currentPackageTool = APT;
    QVERIFY(tool.removeOrphanPackages());

    QCOMPARE(tool.calls.size(), 1);
    QCOMPARE(tool.calls.first().cmd, QStringLiteral("apt-get"));
    QCOMPARE(tool.calls.first().args, (QStringList{"autoremove", "-y"}));
}

void TestPackageToolUninstall::orphan_dnf_callsAutoremoveYes()
{
    CapturingPackageToolLinux tool;
    tool.currentPackageTool = DNF;
    QVERIFY(tool.removeOrphanPackages());
    QCOMPARE(tool.calls.first().cmd, QStringLiteral("dnf"));
    QCOMPARE(tool.calls.first().args, (QStringList{"autoremove", "-y"}));
}

void TestPackageToolUninstall::orphan_yum_callsAutoremoveYes()
{
    CapturingPackageToolLinux tool;
    tool.currentPackageTool = YUM;
    QVERIFY(tool.removeOrphanPackages());
    QCOMPARE(tool.calls.first().cmd, QStringLiteral("yum"));
    QCOMPARE(tool.calls.first().args, (QStringList{"autoremove", "-y"}));
}

void TestPackageToolUninstall::orphan_pacman_queriesThenRemovesEachOrphan()
{
    CapturingPackageToolLinux tool;
    tool.currentPackageTool = PACMAN;
    tool.plainOutputForNextCall = "orphan-a\norphan-b\n";

    QVERIFY(tool.removeOrphanPackages());

    QCOMPARE(tool.calls.size(), 2);
    QCOMPARE(tool.calls.at(0).kind, ExecCall::Plain);
    QCOMPARE(tool.calls.at(0).cmd, QStringLiteral("pacman"));
    QCOMPARE(tool.calls.at(0).args, (QStringList{"-Qdtq"}));

    QCOMPARE(tool.calls.at(1).kind, ExecCall::Sudo);
    QCOMPARE(tool.calls.at(1).cmd, QStringLiteral("pacman"));
    QCOMPARE(tool.calls.at(1).args,
             (QStringList{"-Rns", "--noconfirm", "orphan-a", "orphan-b"}));
}

void TestPackageToolUninstall::orphan_pacman_emptyOutput_skipsRemove()
{
    CapturingPackageToolLinux tool;
    tool.currentPackageTool = PACMAN;
    tool.plainOutputForNextCall = "";   // no orphans found

    QVERIFY(tool.removeOrphanPackages());

    // Only the query ran; no sudo remove command was constructed.
    QCOMPARE(tool.calls.size(), 1);
    QCOMPARE(tool.calls.first().kind, ExecCall::Plain);
}

// ── PackageToolLinux::removeStaleSnapRevisions ────────────────────────────────

void TestPackageToolUninstall::staleSnap_buildsRevisionFlagPerPackage()
{
    CapturingPackageToolLinux tool;
    QList<StaleSnapRevision> revs;
    revs.push_back({"firefox", "4173", "/var/lib/snapd/snaps/firefox_4173.snap", 0});
    revs.push_back({"core20",  "2100", "/var/lib/snapd/snaps/core20_2100.snap", 0});

    QVERIFY(tool.removeStaleSnapRevisions(revs));

    QCOMPARE(tool.calls.size(), 2);
    QCOMPARE(tool.calls.at(0).cmd, QStringLiteral("snap"));
    QCOMPARE(tool.calls.at(0).args,
             (QStringList{"remove", "firefox", "--revision=4173"}));
    QCOMPARE(tool.calls.at(1).args,
             (QStringList{"remove", "core20", "--revision=2100"}));
}

// ── PackageToolMacOS uninstall paths ──────────────────────────────────────────

void TestPackageToolUninstall::brew_uninstall_passesUninstallVerb()
{
    CapturingPackageToolMacOS tool;
    tool.currentPackageTool = HOMEBREW;
    tool.uninstallPackages({"jq"}, /*purge=*/false);

    QCOMPARE(tool.calls.size(), 1);
    QCOMPARE(tool.calls.first().kind, ExecCall::Plain);
    QCOMPARE(tool.calls.first().cmd, QStringLiteral("/opt/homebrew/bin/brew"));
    QCOMPARE(tool.calls.first().args, (QStringList{"uninstall", "jq"}));
}

void TestPackageToolUninstall::brew_uninstall_includesAllPackageNames()
{
    CapturingPackageToolMacOS tool;
    tool.currentPackageTool = HOMEBREW;
    tool.uninstallPackages({"jq", "fzf", "ripgrep"}, /*purge=*/true);

    QCOMPARE(tool.calls.first().args,
             (QStringList{"uninstall", "jq", "fzf", "ripgrep"}));
}

void TestPackageToolUninstall::brew_removeOrphan_callsAutoremove()
{
    CapturingPackageToolMacOS tool;
    QVERIFY(tool.removeOrphanPackages());

    QCOMPARE(tool.calls.size(), 1);
    QCOMPARE(tool.calls.first().cmd, QStringLiteral("/opt/homebrew/bin/brew"));
    QCOMPARE(tool.calls.first().args, (QStringList{"autoremove"}));
}

// ── CommandUtil::buildMacOsSudoShellCommand ───────────────────────────────────

void TestPackageToolUninstall::macSudo_quotesEachArgument()
{
    const QString s = CommandUtil::buildMacOsSudoShellCommand(
        "apt-get", {"remove", "-y", "firefox"});
    QCOMPARE(s, QStringLiteral("apt-get 'remove' '-y' 'firefox'"));
}

void TestPackageToolUninstall::macSudo_escapesEmbeddedSingleQuote()
{
    // A literal single quote becomes '\'' inside the single-quoted segment.
    const QString s = CommandUtil::buildMacOsSudoShellCommand(
        "brew", {"uninstall", "weird's-cask"});
    QCOMPARE(s, QStringLiteral("brew 'uninstall' 'weird'\\\\''s-cask'"));
}

void TestPackageToolUninstall::macSudo_escapesBackslashAndDoubleQuote()
{
    // The AppleScript layer wraps the result in "…" so backslash and " must
    // be escaped before insertion.
    const QString s = CommandUtil::buildMacOsSudoShellCommand(
        "echo", {"a\\b", "say\"hi\""});
    // Single-quoting first, then the trailing pass escapes \\ → \\\\ and " → \"
    QCOMPARE(s, QStringLiteral("echo 'a\\\\b' 'say\\\"hi\\\"'"));
}

void TestPackageToolUninstall::macSudo_preservesPackageNamesWithDashes()
{
    const QString s = CommandUtil::buildMacOsSudoShellCommand(
        "brew", {"uninstall", "google-chrome", "visual-studio-code"});
    QCOMPARE(s, QStringLiteral(
        "brew 'uninstall' 'google-chrome' 'visual-studio-code'"));
}

void TestPackageToolUninstall::macSudo_safelyHandlesShellInjectionAttempt()
{
    // Even if a package name carries shell metacharacters, single-quoting
    // neutralises them — the resulting AppleScript runs `apt-get remove` and
    // a LITERAL `pkg; rm -rf /` argument, not two separate shell commands.
    const QString s = CommandUtil::buildMacOsSudoShellCommand(
        "apt-get", {"remove", "pkg; rm -rf /"});
    QCOMPARE(s, QStringLiteral("apt-get 'remove' 'pkg; rm -rf /'"));
}

QTEST_MAIN(TestPackageToolUninstall)
#include "test_package_tool_uninstall.moc"
