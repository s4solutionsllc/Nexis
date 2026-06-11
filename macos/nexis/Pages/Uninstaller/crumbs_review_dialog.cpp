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
    layout->addWidget(mTable, 1);

    auto *buttons = new QHBoxLayout();
    buttons->addStretch();

    mBtnSkip = new QPushButton(tr("Skip"), this);
    mBtnSkip->setCursor(Qt::PointingHandCursor);
    connect(mBtnSkip, &QPushButton::clicked, this, &QDialog::reject);
    buttons->addWidget(mBtnSkip);

    mBtnDelete = new QPushButton(tr("Delete Selected"), this);
    mBtnDelete->setCursor(Qt::PointingHandCursor);
    mBtnDelete->setAccessibleName("primary");
    connect(mBtnDelete, &QPushButton::clicked, this, &CrumbsReviewDialog::onDeleteSelected);
    buttons->addWidget(mBtnDelete);

    layout->addLayout(buttons);
}

void CrumbsReviewDialog::populate()
{
    const auto crumbs = CrumbsScanner::scanCrumbs(mBundleIds);

    mTable->setRowCount(crumbs.size());
    qint64 total = 0;
    for (int row = 0; row < crumbs.size(); ++row) {
        const auto &c = crumbs.at(row);
        total += c.sizeBytes;

        auto *chkItem = new QTableWidgetItem();
        chkItem->setFlags(chkItem->flags() | Qt::ItemIsUserCheckable);
        chkItem->setCheckState(Qt::Checked);   // default: all selected for delete
        mTable->setItem(row, 0, chkItem);

        auto *pathItem = new QTableWidgetItem(c.path);
        pathItem->setToolTip(c.path);
        mTable->setItem(row, 1, pathItem);

        auto *sizeItem = new QTableWidgetItem(FormatUtil::formatBytes(c.sizeBytes));
        sizeItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        mTable->setItem(row, 2, sizeItem);
    }

    if (crumbs.isEmpty()) {
        mLblSummary->setText(tr("No residual files found for the uninstalled app(s)."));
        mBtnDelete->setEnabled(false);
    } else {
        mLblSummary->setText(tr("Found %1 residual file(s) totalling %2. Uncheck anything you want to keep.")
                              .arg(crumbs.size())
                              .arg(FormatUtil::formatBytes(total)));
    }
}

void CrumbsReviewDialog::onDeleteSelected()
{
    int moved = 0;
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
