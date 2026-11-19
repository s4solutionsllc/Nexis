#ifndef AGENT_WORKTREES_DIALOG_H
#define AGENT_WORKTREES_DIALOG_H

#include <QDialog>
#include <QFuture>
#include <QList>
#include <QString>

#include <Tools/git_worktree_tool.h>

class QLabel;
class QPushButton;
class QTableWidget;

// SSO-27100: review list for git worktrees that AI coding agents left behind
// (see GitWorktreeTool). Interactive only: there is no scheduled or CLI entry
// point, and GitWorktreeTool::removeWorktree() refuses headless runs anyway.
//
// Every row starts unchecked and there is no "Select All". A row git or a
// running process says must not be removed is shown but cannot be checked.
class AgentWorktreesDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AgentWorktreesDialog(QWidget *parent = nullptr);
    // Waits for an in-flight scan/removal so a worker never outlives the dialog.
    ~AgentWorktreesDialog() override;

    void scan();

    // One table row. Pure data so the display rules are testable without a
    // display.
    struct Row {
        GitWorktreeTool::Worktree worktree;
        QString statusLabel;
        QString changesText;
        QString blockReason;   // non-empty: the row cannot be checked
        bool checked = false;  // always false
    };

    static QList<Row> buildRows(const QList<GitWorktreeTool::Worktree> &worktrees);
    static QString statusLabel(const GitWorktreeTool::Worktree &wt);
    static QString changesText(const GitWorktreeTool::Worktree &wt);
    static QString confirmationText(int gitRemovals, int trashMoves);

protected:
    // Test seam: fill the table without running a scan.
    void populate(const QList<GitWorktreeTool::Worktree> &worktrees);

    QTableWidget *table() const { return mTable; }
    QPushButton *removeButton() const { return mBtnRemove; }

private slots:
    void onTableItemChanged();
    void onRemove();

private:
    void buildUI();
    void updateSummary();
    QList<GitWorktreeTool::Worktree> checkedWorktrees() const;

    QLabel *mLblSummary = nullptr;
    QTableWidget *mTable = nullptr;
    QPushButton *mBtnRemove = nullptr;

    QList<GitWorktreeTool::Worktree> mWorktrees;
    QFuture<void> mWorker;
};

#endif // AGENT_WORKTREES_DIALOG_H
