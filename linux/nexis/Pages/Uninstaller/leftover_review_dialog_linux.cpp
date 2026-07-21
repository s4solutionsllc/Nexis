#include "leftover_review_dialog_linux.h"
#include "leftover_review_hook.h"

#include "Services/package_service.h"
#include "Tools/leftover_scanner_linux.h"
#include <Utils/format_util.h>

#include <QCheckBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

// Column indices
static constexpr int COL_CHECK    = 0;
static constexpr int COL_CATEGORY = 1;
static constexpr int COL_PATH     = 2;
static constexpr int COL_SIZE     = 3;
static constexpr int COL_COUNT    = 4;

// Qt::UserRole stores the raw byte count (quint64) for the summary counter.
static constexpr int ROLE_BYTES = Qt::UserRole;

LeftoverReviewDialogLinux::LeftoverReviewDialogLinux(const QStringList &packageNames, QWidget *parent)
    : QDialog(parent),
      mPackageNames(packageNames)
{
    setWindowTitle(tr("Leftover Files"));
    setMinimumSize(700, 440);
    buildUI();
    populate();
}

void LeftoverReviewDialogLinux::buildUI()
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(16, 16, 16, 16);
    root->setSpacing(10);

    // CISO §1 (SSO-15373): one-sentence summary showing size + item count.
    mLblSummary = new QLabel(this);
    mLblSummary->setWordWrap(true);
    root->addWidget(mLblSummary);

    mChkSelectAll = new QCheckBox(tr("Select All"), this);
    connect(mChkSelectAll, &QCheckBox::toggled, this, [this](bool checked) {
        mUpdatingAll = true;
        for (int row = 0; row < mTable->rowCount(); ++row) {
            auto *item = mTable->item(row, COL_CHECK);
            if (item)
                item->setCheckState(checked ? Qt::Checked : Qt::Unchecked);
        }
        mUpdatingAll = false;
        onTableItemChanged();
    });
    root->addWidget(mChkSelectAll);

    mTable = new QTableWidget(this);
    mTable->setColumnCount(COL_COUNT);
    mTable->setHorizontalHeaderLabels({ tr(""), tr("Category"), tr("Path"), tr("Size") });
    mTable->horizontalHeader()->setSectionResizeMode(COL_CHECK,    QHeaderView::ResizeToContents);
    mTable->horizontalHeader()->setSectionResizeMode(COL_CATEGORY, QHeaderView::ResizeToContents);
    mTable->horizontalHeader()->setSectionResizeMode(COL_PATH,     QHeaderView::Stretch);
    mTable->horizontalHeader()->setSectionResizeMode(COL_SIZE,     QHeaderView::ResizeToContents);
    mTable->verticalHeader()->setVisible(false);
    mTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    mTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    connect(mTable, &QTableWidget::itemChanged, this, &LeftoverReviewDialogLinux::onTableItemChanged);
    root->addWidget(mTable, 1);

    auto *buttons = new QHBoxLayout();
    buttons->addStretch();

    mBtnSkip = new QPushButton(tr("Skip"), this);
    mBtnSkip->setCursor(Qt::PointingHandCursor);
    connect(mBtnSkip, &QPushButton::clicked, this, &QDialog::reject);
    buttons->addWidget(mBtnSkip);

    // CISO §1: destructive action uses red/danger accent (SSO-15373).
    // Disabled until at least one item is checked.
    mBtnTrash = new QPushButton(tr("Move to Trash"), this);
    mBtnTrash->setAccessibleName("danger");
    mBtnTrash->setCursor(Qt::PointingHandCursor);
    mBtnTrash->setEnabled(false);
    connect(mBtnTrash, &QPushButton::clicked, this, &LeftoverReviewDialogLinux::onMoveToTrash);
    buttons->addWidget(mBtnTrash);

    root->addLayout(buttons);
}

void LeftoverReviewDialogLinux::populate()
{
    const auto candidates = LeftoverScannerLinux::scanLeftovers(mPackageNames);

    mTotalBytes = 0;
    mItemCount  = candidates.size();
    mTable->setRowCount(mItemCount);

    for (int row = 0; row < candidates.size(); ++row) {
        const auto &c = candidates.at(row);
        mTotalBytes += static_cast<qint64>(c.sizeBytes);

        auto *chkItem = new QTableWidgetItem();
        chkItem->setFlags(chkItem->flags() | Qt::ItemIsUserCheckable);
        chkItem->setCheckState(Qt::Unchecked);   // CISO §5: default unchecked — explicit review required
        chkItem->setData(ROLE_BYTES, QVariant::fromValue(c.sizeBytes));
        mTable->setItem(row, COL_CHECK, chkItem);

        mTable->setItem(row, COL_CATEGORY, new QTableWidgetItem(c.category));

        auto *pathItem = new QTableWidgetItem(c.path);
        pathItem->setToolTip(c.path);
        mTable->setItem(row, COL_PATH, pathItem);

        auto *sizeItem = new QTableWidgetItem(FormatUtil::formatBytes(c.sizeBytes));
        sizeItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        mTable->setItem(row, COL_SIZE, sizeItem);
    }

    updateSummary();
}

void LeftoverReviewDialogLinux::updateSummary()
{
    // Count checked items and their total size.
    int checkedCount    = 0;
    qint64 checkedBytes = 0;
    for (int row = 0; row < mTable->rowCount(); ++row) {
        auto *item = mTable->item(row, COL_CHECK);
        if (item && item->checkState() == Qt::Checked) {
            ++checkedCount;
            checkedBytes += static_cast<qint64>(item->data(ROLE_BYTES).toULongLong());
        }
    }

    if (mItemCount == 0) {
        mLblSummary->setText(tr("No leftover files found for the uninstalled package(s)."));
        mBtnTrash->setEnabled(false);
        return;
    }

    // CISO §1: one sentence max showing size + item count before any destructive action.
    mLblSummary->setText(
        tr("Found %1 leftover file(s) totalling %2 — %3 selected (%4).")
            .arg(mItemCount)
            .arg(FormatUtil::formatBytes(static_cast<quint64>(mTotalBytes)))
            .arg(checkedCount)
            .arg(FormatUtil::formatBytes(static_cast<quint64>(checkedBytes))));

    // CISO AC: "Uninstall Selected" (here "Move to Trash") disabled until
    // at least one item is checked.
    mBtnTrash->setEnabled(checkedCount > 0);

    // Update Select All checkbox tri-state.
    mChkSelectAll->blockSignals(true);
    if (checkedCount == 0)
        mChkSelectAll->setCheckState(Qt::Unchecked);
    else if (checkedCount == mItemCount)
        mChkSelectAll->setCheckState(Qt::Checked);
    else
        mChkSelectAll->setCheckState(Qt::PartiallyChecked);
    mChkSelectAll->blockSignals(false);
}

void LeftoverReviewDialogLinux::onTableItemChanged()
{
    if (mUpdatingAll)
        return;
    updateSummary();
}

void LeftoverReviewDialogLinux::onMoveToTrash()
{
    QStringList toTrash;
    for (int row = 0; row < mTable->rowCount(); ++row) {
        auto *item = mTable->item(row, COL_CHECK);
        if (!item || item->checkState() != Qt::Checked)
            continue;
        // Use the original path for the trash call; PackageToolLinux::trashLeftovers
        // re-canonicalizes and applies the deny-list before acting.
        const QString path = mTable->item(row, COL_PATH)->text();
        if (!path.isEmpty())
            toTrash.append(path);
    }

    if (!toTrash.isEmpty()) {
        // PackageService delegates to PackageToolLinux::trashLeftovers which
        // writes the CISO §3 audit log and uses QFile::moveToTrash (freedesktop.org
        // Trash spec on Linux — never bare unlink, CISO §1 fail-secure).
        PackageService::ins()->trashLeftovers(toTrash);
    }

    accept();
}

namespace LeftoverReviewHook {

void maybeShowReviewDialog(const QStringList &packageNames, QWidget *parent)
{
    // Pre-scan so the dialog doesn't pop up just to say "nothing found".
    if (LeftoverScannerLinux::scanLeftovers(packageNames).isEmpty())
        return;

    auto *dlg = new LeftoverReviewDialogLinux(packageNames, parent);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->open();
}

} // namespace LeftoverReviewHook
