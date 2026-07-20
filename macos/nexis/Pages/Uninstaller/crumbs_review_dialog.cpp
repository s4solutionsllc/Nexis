#include "crumbs_review_dialog.h"

#include "Tools/crumbs_scanner.h"
#include "Utils/format_util.h"

#include <QDialogButtonBox>
#include <QFile>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

CrumbsReviewDialog::CrumbsReviewDialog(const QStringList &bundleIds, QWidget *parent)
    : QDialog(parent),
      mBundleIds(bundleIds)
{
    setWindowTitle(tr("Residual Files"));
    setMinimumSize(640, 400);
    buildUI();
    populate();
}

void CrumbsReviewDialog::buildUI()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(10);

    mLblSummary = new QLabel(this);
    mLblSummary->setWordWrap(true);
    layout->addWidget(mLblSummary);

    mTable = new QTableWidget(this);
    mTable->setColumnCount(3);
    mTable->setHorizontalHeaderLabels({ tr(""), tr("Path"), tr("Size") });
    mTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    mTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    mTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    mTable->verticalHeader()->setVisible(false);
    mTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    mTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    connect(mTable, &QTableWidget::itemChanged, this, &CrumbsReviewDialog::onItemChanged);
    layout->addWidget(mTable, 1);

    auto *buttons = new QHBoxLayout();
    buttons->addStretch();

    mBtnSkip = new QPushButton(tr("Skip"), this);
    mBtnSkip->setCursor(Qt::PointingHandCursor);
    connect(mBtnSkip, &QPushButton::clicked, this, &QDialog::reject);
    buttons->addWidget(mBtnSkip);

    // SSO-15384 / Design Anchor: destructive action button uses the
    // red/destructive accent.  Disabled until at least one item is checked.
    mBtnDelete = new QPushButton(tr("Move to Trash"), this);
    mBtnDelete->setCursor(Qt::PointingHandCursor);
    mBtnDelete->setEnabled(false);
    mBtnDelete->setProperty("buttonRole", "destructive");
    connect(mBtnDelete, &QPushButton::clicked, this, &CrumbsReviewDialog::onDeleteSelected);
    buttons->addWidget(mBtnDelete);

    layout->addLayout(buttons);
}

void CrumbsReviewDialog::populate()
{
    const auto crumbs = CrumbsScanner::scanCrumbs(mBundleIds);

    mTable->blockSignals(true);
    mTable->setRowCount(crumbs.size());
    mTotalBytes = 0;

    for (int row = 0; row < crumbs.size(); ++row) {
        const auto &c = crumbs.at(row);
        mTotalBytes += c.sizeBytes;

        // Store size bytes in UserRole for updateDeleteButton.
        auto *chkItem = new QTableWidgetItem();
        chkItem->setFlags(chkItem->flags() | Qt::ItemIsUserCheckable);
        // SSO-15384 / CISO §5: orphan/leftover matches default to UNCHECKED.
        // The user must explicitly opt-in to deletion of each item.
        chkItem->setCheckState(Qt::Unchecked);
        chkItem->setData(Qt::UserRole, static_cast<qlonglong>(c.sizeBytes));
        mTable->setItem(row, 0, chkItem);

        auto *pathItem = new QTableWidgetItem(c.path);
        pathItem->setToolTip(c.path);
        mTable->setItem(row, 1, pathItem);

        auto *sizeItem = new QTableWidgetItem(FormatUtil::formatBytes(c.sizeBytes));
        sizeItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        mTable->setItem(row, 2, sizeItem);
    }
    mTable->blockSignals(false);

    if (crumbs.isEmpty()) {
        mLblSummary->setText(tr("No residual files found for the uninstalled app(s)."));
        mBtnDelete->setEnabled(false);
    } else {
        // SSO-15384 / Design Anchor: summary shows item count + total size.
        mLblSummary->setText(
            tr("Found %1 residual file(s) totalling %2. Check the items you want to move to Trash.")
                .arg(crumbs.size())
                .arg(FormatUtil::formatBytes(static_cast<quint64>(mTotalBytes))));
    }

    updateDeleteButton();
}

void CrumbsReviewDialog::onItemChanged(QTableWidgetItem *item)
{
    Q_UNUSED(item);
    updateDeleteButton();
}

void CrumbsReviewDialog::updateDeleteButton()
{
    // SSO-15384 / Design Anchor: button stays disabled until at least one
    // checkbox is checked — never a silent no-op.
    int checked = 0;
    qlonglong selectedBytes = 0;
    for (int row = 0; row < mTable->rowCount(); ++row) {
        QTableWidgetItem *chk = mTable->item(row, 0);
        if (chk && chk->checkState() == Qt::Checked) {
            ++checked;
            selectedBytes += chk->data(Qt::UserRole).toLongLong();
        }
    }
    mBtnDelete->setEnabled(checked > 0);
    if (checked > 0) {
        mBtnDelete->setText(
            tr("Move to Trash — %1 item(s), %2")
                .arg(checked)
                .arg(FormatUtil::formatBytes(static_cast<quint64>(selectedBytes))));
    } else {
        mBtnDelete->setText(tr("Move to Trash"));
    }
}

void CrumbsReviewDialog::onDeleteSelected()
{
    int moved  = 0;
    int failed = 0;
    for (int row = 0; row < mTable->rowCount(); ++row) {
        QTableWidgetItem *chk = mTable->item(row, 0);
        if (!chk || chk->checkState() != Qt::Checked)
            continue;

        const QString path = mTable->item(row, 1)->text();
        if (QFile::moveToTrash(path))
            ++moved;
        else
            ++failed;
    }

    Q_UNUSED(moved)
    Q_UNUSED(failed)
    accept();
}
