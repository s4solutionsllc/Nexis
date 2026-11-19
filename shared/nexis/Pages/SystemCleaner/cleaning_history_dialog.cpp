#include "cleaning_history_dialog.h"
#include "Common/dialog_buttons.h"
#include <Utils/format_util.h>

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QPushButton>
#include <QHeaderView>
#include <QMessageBox>
#include <QLocale>

namespace {
constexpr int RunIndexRole = Qt::UserRole;
constexpr int ItemIndexRole = Qt::UserRole + 1;
}

CleaningHistoryDialog::CleaningHistoryDialog(QWidget *parent)
    : QDialog(parent)
{
    buildUI();
    refreshList();
}

QString CleaningHistoryDialog::runSourceLabel(const QString &runSource)
{
    if (runSource == LifecycleAuditLog::RunSource::Interactive)
        return tr("Manual clean");
    if (runSource == LifecycleAuditLog::RunSource::Scheduled)
        return tr("Scheduled clean");
    if (runSource == LifecycleAuditLog::RunSource::Headless)
        return tr("Scheduled clean (background)");
    return tr("App cleanup");
}

QString CleaningHistoryDialog::skipReasonLabel(const QString &skipReason)
{
    if (skipReason == QLatin1String("excluded"))
        return tr("Skipped: excluded by you");
    if (skipReason == QLatin1String("too_recent"))
        return tr("Skipped: too recently modified");
    if (skipReason == QLatin1String("access_denied"))
        return tr("Skipped: access denied");
    if (skipReason == QLatin1String("failed"))
        return tr("Skipped: could not be removed");
    if (skipReason.isEmpty())
        return tr("Skipped");
    return tr("Skipped: %1").arg(skipReason);
}

QString CleaningHistoryDialog::statusLabel(const LifecycleAuditLog::Entry &entry, bool restored)
{
    if (restored)
        return tr("Restored");
    switch (entry.action) {
    case LifecycleAuditLog::Action::MovedToTrash:
        return tr("Moved to Trash");
    case LifecycleAuditLog::Action::PermanentlyDeleted:
        return tr("Deleted");
    case LifecycleAuditLog::Action::Skipped:
        return skipReasonLabel(entry.skipReason);
    case LifecycleAuditLog::Action::Restored:
        return tr("Restored");
    }
    return QString();
}

void CleaningHistoryDialog::buildUI()
{
    setWindowTitle(tr("Recently Removed"));
    setMinimumSize(760, 480);
    setObjectName("cleaningHistoryDialog");

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(12);
    mainLayout->setContentsMargins(DialogButtons::dialogMargins());

    mLblTitle = new QLabel(tr("Recently Removed"));
    mLblTitle->setProperty("accessibleName", "dialog-title");
    mainLayout->addWidget(mLblTitle);

    mLblNotice = new QLabel(tr("Each clean and the paths it removed or skipped. Only items "
                               "moved to the Trash can be restored; deleted items are gone."));
    mLblNotice->setObjectName("lblCleaningHistoryNotice");
    mLblNotice->setWordWrap(true);
    mainLayout->addWidget(mLblNotice);

    mTree = new QTreeWidget;
    mTree->setColumnCount(4);
    mTree->setHeaderLabels({tr("Run / path"), tr("Category"), tr("Size"), tr("Result")});
    mTree->header()->setStretchLastSection(false);
    mTree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    mTree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    mTree->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    mTree->header()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    mTree->setSelectionMode(QAbstractItemView::SingleSelection);
    mTree->setAlternatingRowColors(true);
    mTree->setUniformRowHeights(true);
    mainLayout->addWidget(mTree, 1);

    mBtnRestore = new QPushButton(tr("Restore"));
    mBtnRestore->setObjectName("btnRestoreRemoved");
    mBtnRestore->setToolTip(tr("Move the selected item from the Trash back to where it was"));
    mBtnRestore->setEnabled(false);

    mBtnClose = new QPushButton(tr("Close"));
    mBtnClose->setDefault(true);

    QHBoxLayout *btnRow = new QHBoxLayout;
    btnRow->addWidget(mBtnRestore);
    btnRow->addStretch();
    btnRow->addWidget(mBtnClose);
    mainLayout->addLayout(btnRow);

    connect(mTree, &QTreeWidget::itemSelectionChanged, this, &CleaningHistoryDialog::onSelectionChanged);
    connect(mBtnRestore, &QPushButton::clicked, this, &CleaningHistoryDialog::onRestoreSelected);
    connect(mBtnClose, &QPushButton::clicked, this, &QDialog::accept);
}

void CleaningHistoryDialog::refreshList()
{
    mTree->clear();
    mRuns = LifecycleAuditLog::groupIntoRuns(LifecycleAuditLog::readAll());

    if (mRuns.isEmpty()) {
        QTreeWidgetItem *empty = new QTreeWidgetItem(mTree);
        empty->setText(0, tr("Nothing has been removed yet."));
        empty->setFlags(Qt::NoItemFlags);
        onSelectionChanged();
        return;
    }

    for (int r = 0; r < mRuns.size(); ++r) {
        const LifecycleAuditLog::Run &run = mRuns.at(r);

        QTreeWidgetItem *runItem = new QTreeWidgetItem(mTree);
        runItem->setText(0, tr("%1 — %2")
                                .arg(QLocale().toString(run.timestamp.toLocalTime(), QLocale::ShortFormat),
                                     runSourceLabel(run.runSource)));
        runItem->setText(2, FormatUtil::formatBytes(run.bytesFreed));
        runItem->setText(3, run.skippedCount > 0
                                ? tr("%n removed, %1 skipped", nullptr, run.removedCount).arg(run.skippedCount)
                                : tr("%n removed", nullptr, run.removedCount));
        QFont bold = runItem->font(0);
        bold.setBold(true);
        runItem->setFont(0, bold);

        for (int i = 0; i < run.items.size(); ++i) {
            const LifecycleAuditLog::Entry &entry = run.items.at(i);
            const bool restored = entry.action == LifecycleAuditLog::Action::MovedToTrash
                && run.restoredDestinations.contains(entry.trashDestination);

            QTreeWidgetItem *child = new QTreeWidgetItem(runItem);
            child->setText(0, entry.originalPath);
            child->setToolTip(0, entry.originalPath);
            child->setText(1, entry.category);
            child->setText(2, entry.action == LifecycleAuditLog::Action::Skipped
                                  ? QString()
                                  : FormatUtil::formatBytes(entry.sizeBytes));
            child->setText(3, statusLabel(entry, restored));
            child->setData(0, RunIndexRole, r);
            child->setData(0, ItemIndexRole, i);
        }
    }

    mTree->expandItem(mTree->topLevelItem(0));
    onSelectionChanged();
}

const LifecycleAuditLog::Entry *CleaningHistoryDialog::selectedRestorableEntry() const
{
    const QTreeWidgetItem *item = mTree->currentItem();
    if (!item || !item->data(0, ItemIndexRole).isValid())
        return nullptr;

    const LifecycleAuditLog::Run &run = mRuns.at(item->data(0, RunIndexRole).toInt());
    const LifecycleAuditLog::Entry &entry = run.items.at(item->data(0, ItemIndexRole).toInt());
    return run.isRestorable(entry) ? &entry : nullptr;
}

void CleaningHistoryDialog::onSelectionChanged()
{
    mBtnRestore->setEnabled(selectedRestorableEntry() != nullptr);
}

void CleaningHistoryDialog::onRestoreSelected()
{
    const LifecycleAuditLog::Entry *entry = selectedRestorableEntry();
    if (!entry)
        return;

    const LifecycleAuditLog::RestoreResult result = LifecycleAuditLog::restore(*entry);
    if (!result.ok()) {
        QMessageBox::warning(this, tr("Cannot restore"), result.message);
        return;
    }
    refreshList();
}
