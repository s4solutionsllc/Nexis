// FW-07 (SSO-3735): fixture tests for the apt-3.1 history/why parsers + the
// command-construction seam for PackageToolLinux::aptHistory*. Mirrors
// test_package_tool.cpp (parsers) and test_package_tool_uninstall.cpp (argv
// capture via CapturingPackageToolLinux).

#include <QStringList>
#include <QTest>

#include "Tools/package_tool_shared.h"
#include "package_tool_linux.h"

namespace {

struct ExecCall {
    enum Kind { Sudo, Plain };
    Kind kind;
    QString cmd;
    QStringList args;
    QString stubOutput;
};

class CapturingPackageToolLinux : public PackageToolLinux
{
public:
    QList<ExecCall> calls;
    QString plainOutputForNextCall;

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

} // namespace

class TestAptHistory : public QObject
{
    Q_OBJECT

private slots:
    // parseAptVersion
    void version_apt_3_1_0();
    void version_apt_2_8_5();
    void version_apt_3_1_with_no_patch();
    void version_unparseable();

    // parseAptHistoryList
    void historyList_dnfStyleTable();
    void historyList_skipsSeparatorAndHeader();
    void historyList_emptyOutput();
    void historyList_ignoresMissingId();

    // parseAptHistoryInfo
    void historyInfo_labelledHeaderAndSections();
    void historyInfo_stripsArchSuffix();
    void historyInfo_inlineSectionList();
    void historyInfo_emptyOutput();

    // parseAptWhy
    void why_singleLineInstalledByUser();
    void why_chainedReasons();
    void why_stripsBoilerplateNoise();

    // Command construction (live API)
    void apt_history_not_supported_returnsEmpty();
    void apt_historyList_buildsCorrectArgs();
    void apt_historyInfo_buildsCorrectArgs();
    void apt_why_usesWhyVerb();
    void apt_whyNot_usesWhyNotVerb();
    void apt_why_rejectsEmptyPackageName();
    void apt_historyUndo_buildsSudoArgs();
    void apt_historyUndo_rejectsNonPositiveId();
    void apt_historyRollback_buildsSudoArgs();
};

// ── parseAptVersion ───────────────────────────────────────────────────────────

void TestAptHistory::version_apt_3_1_0()
{
    AptVersion v = PackageTool::parseAptVersion("apt 3.1.0 (amd64)\n");
    QVERIFY(v.valid);
    QCOMPARE(v.major, 3);
    QCOMPARE(v.minor, 1);
    QCOMPARE(v.patch, 0);
    QVERIFY(v.atLeast(3, 1, 0));
    QVERIFY(!v.atLeast(3, 2));
}

void TestAptHistory::version_apt_2_8_5()
{
    AptVersion v = PackageTool::parseAptVersion("apt 2.8.5 (amd64)\n");
    QVERIFY(v.valid);
    QCOMPARE(v.major, 2);
    QCOMPARE(v.minor, 8);
    QVERIFY(!v.atLeast(3, 1, 0));
}

void TestAptHistory::version_apt_3_1_with_no_patch()
{
    AptVersion v = PackageTool::parseAptVersion("apt 3.1 (amd64)\n");
    QVERIFY(v.valid);
    QCOMPARE(v.major, 3);
    QCOMPARE(v.minor, 1);
    QCOMPARE(v.patch, 0);
    QVERIFY(v.atLeast(3, 1, 0));
}

void TestAptHistory::version_unparseable()
{
    AptVersion v = PackageTool::parseAptVersion("garbage output");
    QVERIFY(!v.valid);
    QVERIFY(!v.atLeast(0, 0, 0));
}

// ── parseAptHistoryList ───────────────────────────────────────────────────────

static const char *kHistoryListDnfStyle =
    "ID | Date and time        | Operation | Command line\n"
    "---+----------------------+-----------+-----------------------------\n"
    "12 | 2026-06-10 14:32:11  | install   | apt install firefox\n"
    "11 | 2026-06-09 09:15:02  | upgrade   | apt upgrade\n"
    "10 | 2026-06-08 22:01:55  | remove    | apt remove neovim\n";

void TestAptHistory::historyList_dnfStyleTable()
{
    QList<AptHistoryEntry> entries = PackageTool::parseAptHistoryList(kHistoryListDnfStyle);
    QCOMPARE(entries.size(), 3);
    QCOMPARE(entries[0].id, 12);
    QCOMPARE(entries[0].dateTime, QString("2026-06-10 14:32:11"));
    QCOMPARE(entries[0].operation, QString("install"));
    QCOMPARE(entries[0].commandLine, QString("apt install firefox"));
    QCOMPARE(entries[1].id, 11);
    QCOMPARE(entries[2].operation, QString("remove"));
}

void TestAptHistory::historyList_skipsSeparatorAndHeader()
{
    QList<AptHistoryEntry> entries = PackageTool::parseAptHistoryList(
        "------------------------\n"
        "ID | Date | Operation\n"
        "---+------+----------\n"
        "1  | 2026-06-01 | install\n");
    QCOMPARE(entries.size(), 1);
    QCOMPARE(entries[0].id, 1);
}

void TestAptHistory::historyList_emptyOutput()
{
    QList<AptHistoryEntry> entries = PackageTool::parseAptHistoryList(QString());
    QCOMPARE(entries.size(), 0);
}

void TestAptHistory::historyList_ignoresMissingId()
{
    // A genuine pipe-separated line that has no numeric id (a malformed locale
    // header for example) should not produce a phantom entry.
    QList<AptHistoryEntry> entries = PackageTool::parseAptHistoryList(
        "ID | Date | Operation\n"
        "abc | 2026-06-01 | install\n");
    QCOMPARE(entries.size(), 0);
}

// ── parseAptHistoryInfo ───────────────────────────────────────────────────────

static const char *kHistoryInfoLabelled =
    "Transaction ID    : 12\n"
    "Begin time        : 2026-06-10 14:32:11\n"
    "User              : alice (1000)\n"
    "Command line      : apt install firefox\n"
    "Operation         : install\n"
    "Installed:\n"
    "  firefox:amd64 130.0-1\n"
    "  firefox-locale-en:amd64 130.0-1\n"
    "Upgraded:\n"
    "  libnss3:amd64 3.98-1\n";

void TestAptHistory::historyInfo_labelledHeaderAndSections()
{
    AptHistoryEntry e = PackageTool::parseAptHistoryInfo(kHistoryInfoLabelled);
    QCOMPARE(e.id, 12);
    QCOMPARE(e.dateTime, QString("2026-06-10 14:32:11"));
    QCOMPARE(e.user, QString("alice (1000)"));
    QCOMPARE(e.operation, QString("install"));
    QCOMPARE(e.commandLine, QString("apt install firefox"));
    QCOMPARE(e.packages, (QStringList{"firefox", "firefox-locale-en", "libnss3"}));
}

void TestAptHistory::historyInfo_stripsArchSuffix()
{
    AptHistoryEntry e = PackageTool::parseAptHistoryInfo(
        "Transaction ID: 1\n"
        "Installed:\n"
        "  libfoo:amd64 1.0\n"
        "  libbar:i386 2.0\n");
    QCOMPARE(e.packages, (QStringList{"libfoo", "libbar"}));
}

void TestAptHistory::historyInfo_inlineSectionList()
{
    AptHistoryEntry e = PackageTool::parseAptHistoryInfo(
        "Transaction ID: 5\n"
        "Removed: pkg-a pkg-b pkg-c\n");
    QCOMPARE(e.id, 5);
    QCOMPARE(e.packages, (QStringList{"pkg-a", "pkg-b", "pkg-c"}));
}

void TestAptHistory::historyInfo_emptyOutput()
{
    AptHistoryEntry e = PackageTool::parseAptHistoryInfo(QString());
    QCOMPARE(e.id, 0);
    QCOMPARE(e.packages.size(), 0);
}

// ── parseAptWhy ───────────────────────────────────────────────────────────────

void TestAptHistory::why_singleLineInstalledByUser()
{
    QStringList r = PackageTool::parseAptWhy("firefox  Installed by user\n");
    QCOMPARE(r.size(), 1);
    QCOMPARE(r[0], QString("firefox  Installed by user"));
}

void TestAptHistory::why_chainedReasons()
{
    QStringList r = PackageTool::parseAptWhy(
        "firefox  Required by:\n"
        "  thunderbird depends on firefox\n"
        "  ubuntu-desktop depends on firefox\n");
    QCOMPARE(r.size(), 3);
    QVERIFY(r[1].contains("thunderbird"));
}

void TestAptHistory::why_stripsBoilerplateNoise()
{
    // "Reading package lists…" / "Building dependency tree…" should not show
    // up in the user-facing reasons list.
    QStringList r = PackageTool::parseAptWhy(
        "Reading package lists... Done\n"
        "Building dependency tree... Done\n"
        "Reading state information... Done\n"
        "firefox  Installed by user\n");
    QCOMPARE(r.size(), 1);
    QCOMPARE(r[0], QString("firefox  Installed by user"));
}

// ── Command construction (live API) ──────────────────────────────────────────

void TestAptHistory::apt_history_not_supported_returnsEmpty()
{
    CapturingPackageToolLinux tool;
    tool.currentPackageTool = APT;
    // No version output → parseAptVersion returns invalid → atLeast(3,1) false.
    tool.plainOutputForNextCall = "apt 2.8.5 (amd64)\n";
    QVERIFY(!tool.aptHistorySupported());
    QCOMPARE(tool.calls.size(), 1);
    QCOMPARE(tool.calls.first().cmd, QStringLiteral("apt"));
    QCOMPARE(tool.calls.first().args, (QStringList{"--version"}));

    // Subsequent live calls short-circuit without invoking apt history-*.
    tool.calls.clear();
    tool.plainOutputForNextCall = "apt 2.8.5 (amd64)\n";
    QCOMPARE(tool.getAptHistory().size(), 0);
    // Only the version check ran — no history-list call was made.
    QCOMPARE(tool.calls.size(), 1);
    QCOMPARE(tool.calls.first().args, (QStringList{"--version"}));
}

void TestAptHistory::apt_historyList_buildsCorrectArgs()
{
    CapturingPackageToolLinux tool;
    tool.currentPackageTool = APT;
    tool.plainOutputForNextCall = "apt 3.1.0 (amd64)\n";
    // After version check succeeds the next runCommand will get the (empty)
    // history-list output. Use a separate inline trick: prime version, then
    // override for the next call.
    QVERIFY(tool.aptHistorySupported());

    tool.calls.clear();
    // Two reads in sequence: aptHistorySupported() re-checks version, then
    // runs history-list. Both consume plainOutputForNextCall; prime the
    // second call by hand-feeding before each runCommand.
    tool.plainOutputForNextCall = "apt 3.1.0 (amd64)\n";
    QList<AptHistoryEntry> entries = tool.getAptHistory();
    Q_UNUSED(entries);

    // Two recorded calls: --version, then history-list.
    QCOMPARE(tool.calls.size(), 2);
    QCOMPARE(tool.calls.at(0).args, (QStringList{"--version"}));
    QCOMPARE(tool.calls.at(1).cmd, QStringLiteral("apt"));
    QCOMPARE(tool.calls.at(1).args, (QStringList{"history-list"}));
}

void TestAptHistory::apt_historyInfo_buildsCorrectArgs()
{
    CapturingPackageToolLinux tool;
    tool.currentPackageTool = APT;
    tool.plainOutputForNextCall = "apt 3.1.0 (amd64)\n";
    AptHistoryEntry e = tool.getAptHistoryInfo(42);
    Q_UNUSED(e);

    QCOMPARE(tool.calls.size(), 2);
    QCOMPARE(tool.calls.at(1).cmd, QStringLiteral("apt"));
    QCOMPARE(tool.calls.at(1).args, (QStringList{"history-info", "42"}));
}

void TestAptHistory::apt_why_usesWhyVerb()
{
    CapturingPackageToolLinux tool;
    tool.currentPackageTool = APT;
    tool.plainOutputForNextCall = "apt 3.1.0 (amd64)\n";
    QStringList r = tool.aptWhy("firefox", /*whyNot=*/false);
    Q_UNUSED(r);

    QCOMPARE(tool.calls.size(), 2);
    QCOMPARE(tool.calls.at(1).args, (QStringList{"why", "firefox"}));
}

void TestAptHistory::apt_whyNot_usesWhyNotVerb()
{
    CapturingPackageToolLinux tool;
    tool.currentPackageTool = APT;
    tool.plainOutputForNextCall = "apt 3.1.0 (amd64)\n";
    QStringList r = tool.aptWhy("vim", /*whyNot=*/true);
    Q_UNUSED(r);

    QCOMPARE(tool.calls.size(), 2);
    QCOMPARE(tool.calls.at(1).args, (QStringList{"why-not", "vim"}));
}

void TestAptHistory::apt_why_rejectsEmptyPackageName()
{
    CapturingPackageToolLinux tool;
    tool.currentPackageTool = APT;
    tool.plainOutputForNextCall = "apt 3.1.0 (amd64)\n";
    QStringList r = tool.aptWhy("", false);
    QCOMPARE(r.size(), 0);
    // Only the version probe ran — never asked apt to explain nothing.
    QCOMPARE(tool.calls.size(), 1);
}

void TestAptHistory::apt_historyUndo_buildsSudoArgs()
{
    CapturingPackageToolLinux tool;
    tool.currentPackageTool = APT;
    tool.plainOutputForNextCall = "apt 3.1.0 (amd64)\n";
    QVERIFY(tool.aptHistoryUndo(7));

    QCOMPARE(tool.calls.size(), 2);
    QCOMPARE(tool.calls.at(1).kind, ExecCall::Sudo);
    QCOMPARE(tool.calls.at(1).cmd, QStringLiteral("apt"));
    QCOMPARE(tool.calls.at(1).args, (QStringList{"history-undo", "-y", "7"}));
}

void TestAptHistory::apt_historyUndo_rejectsNonPositiveId()
{
    CapturingPackageToolLinux tool;
    tool.currentPackageTool = APT;
    tool.plainOutputForNextCall = "apt 3.1.0 (amd64)\n";
    QVERIFY(!tool.aptHistoryUndo(0));
    QVERIFY(!tool.aptHistoryUndo(-3));
    // Version probes only — no sudo call was ever assembled with id 0 / -3.
    int sudo = 0;
    for (const ExecCall &c : tool.calls)
        if (c.kind == ExecCall::Sudo) ++sudo;
    QCOMPARE(sudo, 0);
}

void TestAptHistory::apt_historyRollback_buildsSudoArgs()
{
    CapturingPackageToolLinux tool;
    tool.currentPackageTool = APT;
    tool.plainOutputForNextCall = "apt 3.1.0 (amd64)\n";
    QVERIFY(tool.aptHistoryRollback(3));

    QCOMPARE(tool.calls.size(), 2);
    QCOMPARE(tool.calls.at(1).kind, ExecCall::Sudo);
    QCOMPARE(tool.calls.at(1).args, (QStringList{"history-rollback", "-y", "3"}));
}

QTEST_MAIN(TestAptHistory)
#include "test_apt_history.moc"
