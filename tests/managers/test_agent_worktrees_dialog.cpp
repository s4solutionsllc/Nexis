// SSO-27100: AgentWorktreesDialog's pure display rules, tested without
// instantiating the QDialog (same convention as test_orphan_leftovers_dialog):
//   - every row starts unchecked (no auto-selection, ever);
//   - a row that GitWorktreeTool says is blocked carries the reason, so the
//     dialog disables its checkbox;
//   - the Changes column shows dirty and unpushed counts, and "Unknown" where
//     git cannot see the folder;
//   - the confirmation is one sentence naming the git/Trash split.

#include <QtTest>

#include "Pages/DiskTools/agent_worktrees_dialog.h"

using GitWorktreeTool::Worktree;

namespace {

Worktree make(Worktree::Kind kind, int dirty, int unpushed, bool inUse = false)
{
    Worktree wt;
    wt.kind = kind;
    wt.path = QStringLiteral("/h/p/.claude/worktrees/x");
    wt.dirtyCount = dirty;
    wt.unpushedCount = unpushed;
    wt.inUse = inUse;
    wt.folderExists = kind != Worktree::Kind::MissingFolder;
    return wt;
}

} // namespace

class TestAgentWorktreesDialog : public QObject
{
    Q_OBJECT

private slots:
    void buildRows_neverPreselects();
    void buildRows_carriesBlockReason();
    void changesText_perKind();
    void statusLabel_distinctPerKind();
    void confirmationText_namesTheSplit();
};

void TestAgentWorktreesDialog::buildRows_neverPreselects()
{
    const auto rows = AgentWorktreesDialog::buildRows({
        make(Worktree::Kind::MissingFolder, -1, -1),
        make(Worktree::Kind::Unregistered, -1, -1),
        make(Worktree::Kind::Idle, 0, 0),
    });
    QCOMPARE(rows.size(), 3);
    for (const auto &row : rows)
        QVERIFY(!row.checked);
}

void TestAgentWorktreesDialog::buildRows_carriesBlockReason()
{
    const auto rows = AgentWorktreesDialog::buildRows({
        make(Worktree::Kind::Idle, 0, 0),
        make(Worktree::Kind::Idle, 2, 0),
        make(Worktree::Kind::Unregistered, -1, -1, /*inUse=*/true),
    });
    QVERIFY(rows.at(0).blockReason.isEmpty());
    QVERIFY(!rows.at(1).blockReason.isEmpty());
    QVERIFY(!rows.at(2).blockReason.isEmpty());
}

void TestAgentWorktreesDialog::changesText_perKind()
{
    QCOMPARE(AgentWorktreesDialog::changesText(make(Worktree::Kind::Idle, 0, -1)),
             QStringLiteral("Clean"));
    QCOMPARE(AgentWorktreesDialog::changesText(make(Worktree::Kind::Idle, 3, 2)),
             QStringLiteral("3 uncommitted, 2 unpushed"));
    QCOMPARE(AgentWorktreesDialog::changesText(make(Worktree::Kind::Idle, 0, 1)),
             QStringLiteral("Clean, 1 unpushed"));
    QCOMPARE(AgentWorktreesDialog::changesText(make(Worktree::Kind::Idle, -1, -1)),
             QStringLiteral("Unknown"));
    QCOMPARE(AgentWorktreesDialog::changesText(make(Worktree::Kind::Unregistered, -1, -1)),
             QStringLiteral("Unknown"));
    QCOMPARE(AgentWorktreesDialog::changesText(make(Worktree::Kind::MissingFolder, -1, -1)),
             QStringLiteral("—"));
}

void TestAgentWorktreesDialog::statusLabel_distinctPerKind()
{
    const QStringList labels = {
        AgentWorktreesDialog::statusLabel(make(Worktree::Kind::MissingFolder, -1, -1)),
        AgentWorktreesDialog::statusLabel(make(Worktree::Kind::Unregistered, -1, -1)),
        AgentWorktreesDialog::statusLabel(make(Worktree::Kind::Idle, 0, 0)),
    };
    QCOMPARE(QSet<QString>(labels.begin(), labels.end()).size(), 3);
}

void TestAgentWorktreesDialog::confirmationText_namesTheSplit()
{
    QCOMPARE(AgentWorktreesDialog::confirmationText(2, 1),
             QStringLiteral("Remove 3 worktree(s), 2 with git and 1 to the Trash?"));
}

QTEST_MAIN(TestAgentWorktreesDialog)
#include "test_agent_worktrees_dialog.moc"
