#include "agent_worktrees_dialog.h"
#include "Common/dialog_buttons.h"

#include <Utils/format_util.h>

#include <QDir>
#include <QHeaderView>
#include <QLabel>
#include <QLocale>
#include <QMessageBox>
#include <QMetaObject>
#include <QPointer>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>
#include <QtConcurrent>

using GitWorktreeTool::Worktree;

namespace {
constexpr int COL_CHECK = 0;
constexpr int COL_STATUS = 1;
constexpr int COL_PATH = 2;
constexpr int COL_BRANCH = 3;
constexpr int COL_CHANGES = 4;
constexpr int COL_SIZE = 5;
constexpr int COL_ACTIVITY = 6;
constexpr int COL_COUNT = 7;

// Index into mWorktrees, stored on the checkbox cell.
constexpr int ROLE_INDEX = Qt::UserRole;
}

AgentWorktreesDialog::AgentWorktreesDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Agent Worktrees"));
    setMinimumSize(900, 460);
    buildUI();
}

AgentWorktreesDialog::~AgentWorktreesDialog()
{
    mWorker.waitForFinished();
}

void AgentWorktreesDialog::buildUI()
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(DialogButtons::dialogMargins());
    root->setSpacing(10);

    auto *intro = new QLabel(
        tr("Git worktrees left behind by AI coding agents (Claude Code, Codex, Cursor, Paperclip). "
           "Registered worktrees are removed with git, which refuses any that have uncommitted "
           "changes. Folders their repository no longer registers cannot be inspected, so they "
           "move to the Trash and can be restored from System Cleaner → History."),
        this);
    intro->setWordWrap(true);
    root->addWidget(intro);

    mLblSummary = new QLabel(this);
    mLblSummary->setWordWrap(true);
    root->addWidget(mLblSummary);

    mTable = new QTableWidget(this);
    mTable->setColumnCount(COL_COUNT);
    mTable->setHorizontalHeaderLabels({QString(), tr("Status"), tr("Path"), tr("Branch"),
                                       tr("Changes"), tr("Size"), tr("Last activity")});
    auto *header = mTable->horizontalHeader();
    header->setSectionResizeMode(COL_CHECK, QHeaderView::ResizeToContents);
    header->setSectionResizeMode(COL_STATUS, QHeaderView::ResizeToContents);
    header->setSectionResizeMode(COL_PATH, QHeaderView::Stretch);
    header->setSectionResizeMode(COL_BRANCH, QHeaderView::ResizeToContents);
    header->setSectionResizeMode(COL_CHANGES, QHeaderView::ResizeToContents);
    header->setSectionResizeMode(COL_SIZE, QHeaderView::ResizeToContents);
    header->setSectionResizeMode(COL_ACTIVITY, QHeaderView::ResizeToContents);
    mTable->verticalHeader()->setVisible(false);
    mTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    mTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    connect(mTable, &QTableWidget::itemChanged, this, &AgentWorktreesDialog::onTableItemChanged);
    root->addWidget(mTable, 1);

    DialogButtons::Row row = DialogButtons::build(this, tr("Remove Selected"),
                                                  DialogButtons::Confirm::Danger, tr("Close"));
    mBtnRemove = row.confirm;
    mBtnRemove->setEnabled(false);
    connect(mBtnRemove, &QPushButton::clicked, this, &AgentWorktreesDialog::onRemove);
    root->addWidget(row.box);

    mLblSummary->setText(tr("Scanning for agent worktrees…"));
    mTable->setEnabled(false);
}

void AgentWorktreesDialog::scan()
{
    mTable->setEnabled(false);
    mBtnRemove->setEnabled(false);
    mLblSummary->setText(tr("Scanning for agent worktrees…"));

    // The scan runs git and walks folders, so it can outlive the dialog:
    // capture a QPointer, not `this`, and re-check it in the queued slot.
    QPointer<AgentWorktreesDialog> self(this);
    mWorker = QtConcurrent::run([self]() {
        if (!GitWorktreeTool::gitAvailable()) {
            if (self)
                QMetaObject::invokeMethod(self.data(), [self]() {
                    if (self)
                        self->mLblSummary->setText(tr("git is not installed, so worktrees cannot be listed."));
                }, Qt::QueuedConnection);
            return;
        }
        const QList<Worktree> found =
            GitWorktreeTool::scan(GitWorktreeTool::ScanOptions::forHome(QDir::homePath()));
        if (!self)
            return;
        QMetaObject::invokeMethod(self.data(), [self, found]() {
            if (!self)
                return;
            self->mTable->setEnabled(true);
            self->populate(found);
        }, Qt::QueuedConnection);
    });
}

QString AgentWorktreesDialog::statusLabel(const Worktree &wt)
{
    switch (wt.kind) {
    case Worktree::Kind::MissingFolder: return tr("Folder gone");
    case Worktree::Kind::Unregistered:  return tr("Not registered");
    case Worktree::Kind::Idle:          return tr("Idle");
    }
    return QString();
}

QString AgentWorktreesDialog::changesText(const Worktree &wt)
{
    switch (wt.kind) {
    case Worktree::Kind::MissingFolder:
        return QStringLiteral("—");
    case Worktree::Kind::Unregistered:
        return tr("Unknown");
    case Worktree::Kind::Idle:
        break;
    }
    QStringList parts;
    if (wt.dirtyCount < 0)
        parts << tr("Unknown");
    else if (wt.dirtyCount == 0)
        parts << tr("Clean");
    else
        parts << tr("%1 uncommitted").arg(wt.dirtyCount);
    if (wt.unpushedCount > 0)
        parts << tr("%1 unpushed").arg(wt.unpushedCount);
    return parts.join(QStringLiteral(", "));
}

QList<AgentWorktreesDialog::Row> AgentWorktreesDialog::buildRows(const QList<Worktree> &worktrees)
{
    QList<Row> rows;
    rows.reserve(worktrees.size());
    for (const Worktree &wt : worktrees) {
        Row row;
        row.worktree = wt;
        row.statusLabel = statusLabel(wt);
        row.changesText = changesText(wt);
        row.blockReason = GitWorktreeTool::blockReason(wt);
        row.checked = false;
        rows.append(row);
    }
    return rows;
}

QString AgentWorktreesDialog::confirmationText(int gitRemovals, int trashMoves)
{
    const int total = gitRemovals + trashMoves;
    return tr("Remove %n worktree(s), %1 with git and %2 to the Trash?", nullptr, total)
        .arg(gitRemovals)
        .arg(trashMoves);
}

void AgentWorktreesDialog::populate(const QList<Worktree> &worktrees)
{
    mWorktrees = worktrees;
    const QList<Row> rows = buildRows(worktrees);

    mTable->blockSignals(true);
    mTable->setRowCount(rows.size());
    for (int i = 0; i < rows.size(); ++i) {
        const Row &r = rows.at(i);
        const Worktree &wt = r.worktree;

        auto *chk = new QTableWidgetItem();
        chk->setData(ROLE_INDEX, i);
        if (r.blockReason.isEmpty()) {
            chk->setFlags(chk->flags() | Qt::ItemIsUserCheckable);
            chk->setCheckState(Qt::Unchecked);
        } else {
            chk->setFlags(chk->flags() & ~Qt::ItemIsEnabled & ~Qt::ItemIsUserCheckable);
            chk->setToolTip(r.blockReason);
        }
        mTable->setItem(i, COL_CHECK, chk);

        auto *status = new QTableWidgetItem(r.statusLabel);
        if (!r.blockReason.isEmpty())
            status->setToolTip(r.blockReason);
        mTable->setItem(i, COL_STATUS, status);

        auto *path = new QTableWidgetItem(wt.path);
        path->setToolTip(wt.repoPath.isEmpty() ? wt.path : tr("%1\nRepository: %2").arg(wt.path, wt.repoPath));
        mTable->setItem(i, COL_PATH, path);

        mTable->setItem(i, COL_BRANCH, new QTableWidgetItem(wt.branch));
        mTable->setItem(i, COL_CHANGES, new QTableWidgetItem(r.changesText));

        auto *size = new QTableWidgetItem(wt.folderExists ? FormatUtil::formatBytes(wt.sizeBytes) : QString());
        size->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        mTable->setItem(i, COL_SIZE, size);

        mTable->setItem(i, COL_ACTIVITY, new QTableWidgetItem(
            wt.lastActivity.isValid() ? QLocale().toString(wt.lastActivity.date(), QLocale::ShortFormat)
                                      : QString()));
    }
    mTable->blockSignals(false);
    updateSummary();
}

QList<Worktree> AgentWorktreesDialog::checkedWorktrees() const
{
    QList<Worktree> out;
    for (int row = 0; row < mTable->rowCount(); ++row) {
        const auto *chk = mTable->item(row, COL_CHECK);
        if (chk && chk->checkState() == Qt::Checked)
            out.append(mWorktrees.at(chk->data(ROLE_INDEX).toInt()));
    }
    return out;
}

void AgentWorktreesDialog::updateSummary()
{
    if (mWorktrees.isEmpty()) {
        mLblSummary->setText(tr("No abandoned agent worktrees found."));
        mBtnRemove->setEnabled(false);
        return;
    }

    quint64 totalBytes = 0;
    for (const Worktree &wt : std::as_const(mWorktrees))
        totalBytes += wt.sizeBytes;
    const QList<Worktree> checked = checkedWorktrees();
    quint64 checkedBytes = 0;
    for (const Worktree &wt : checked)
        checkedBytes += wt.sizeBytes;

    mLblSummary->setText(
        tr("Found %1 worktree(s) using %2 — %3 selected (%4).")
            .arg(mWorktrees.size())
            .arg(FormatUtil::formatBytes(totalBytes))
            .arg(checked.size())
            .arg(FormatUtil::formatBytes(checkedBytes)));
    mBtnRemove->setEnabled(!checked.isEmpty());
}

void AgentWorktreesDialog::onTableItemChanged()
{
    updateSummary();
}

void AgentWorktreesDialog::onRemove()
{
    const QList<Worktree> chosen = checkedWorktrees();
    if (chosen.isEmpty())
        return;

    int trashMoves = 0;
    for (const Worktree &wt : chosen)
        trashMoves += wt.kind == Worktree::Kind::Unregistered ? 1 : 0;

    const auto choice = QMessageBox::question(
        this, tr("Remove Worktrees"), confirmationText(chosen.size() - trashMoves, trashMoves),
        QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
    if (choice != QMessageBox::Yes)
        return;

    mTable->setEnabled(false);
    mBtnRemove->setEnabled(false);
    mLblSummary->setText(tr("Removing…"));

    QPointer<AgentWorktreesDialog> self(this);
    mWorker = QtConcurrent::run([self, chosen]() {
        QStringList failures;
        for (const Worktree &wt : chosen) {
            const GitWorktreeTool::RemoveResult r = GitWorktreeTool::removeWorktree(wt);
            if (!r.ok)
                failures << QStringLiteral("%1: %2").arg(wt.path, r.message);
        }
        if (!self)
            return;
        QMetaObject::invokeMethod(self.data(), [self, failures]() {
            if (!self)
                return;
            if (!failures.isEmpty())
                QMessageBox::warning(self.data(), tr("Remove Worktrees"),
                                     tr("Some worktrees were left in place:\n\n%1").arg(failures.join(QLatin1Char('\n'))));
            self->scan();
        }, Qt::QueuedConnection);
    });
}
