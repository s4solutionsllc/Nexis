#include "orphan_leftovers_dialog.h"

#include "Services/package_service.h"
#include <Utils/format_util.h>

#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QMetaObject>
#include <QPointer>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>
#include <QtConcurrent>

namespace {
constexpr int COL_CHECK    = 0;
constexpr int COL_CATEGORY = 1;
constexpr int COL_PATH     = 2;
constexpr int COL_SIZE     = 3;
constexpr int COL_SIGNALS  = 4;
constexpr int COL_COUNT    = 5;

// Qt::UserRole on the checkbox cell stores the raw byte count so the
// summary/confirmation copy can total the *checked* subset without re-
// parsing the formatted size text.
constexpr int ROLE_BYTES = Qt::UserRole;
}

OrphanLeftoversDialog::OrphanLeftoversDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Orphan Leftovers"));
    setMinimumSize(760, 460);
    buildUI();
}

OrphanLeftoversDialog::~OrphanLeftoversDialog()
{
    // SSO-3362-class backstop: block until the in-flight scan (if any)
    // finishes so the QPointer-guarded worker in scan() never observes a
    // half-destroyed object. See MaintenanceWizardDialog for the identical
    // pattern.
    mScanFuture.waitForFinished();
}

void OrphanLeftoversDialog::buildUI()
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(16, 16, 16, 16);
    root->setSpacing(10);

    // CISO §1 (SSO-15373): size + item-count summary, always visible before
    // any destructive action — not just shown once inside a confirm popup.
    mLblSummary = new QLabel(this);
    mLblSummary->setWordWrap(true);
    root->addWidget(mLblSummary);

    // Deliberately no "Select All" control here (CISO Psychological
    // Acceptability gate, SSO-15429) — unlike LeftoverReviewDialogLinux /
    // CrumbsReviewDialog, orphan matches have no known just-uninstalled app
    // to correlate against, so every item requires individual review.
    mTable = new QTableWidget(this);
    mTable->setColumnCount(COL_COUNT);
    mTable->setHorizontalHeaderLabels({ tr(""), tr("Category"), tr("Path"), tr("Size"), tr("Matched Signals") });
    mTable->horizontalHeader()->setSectionResizeMode(COL_CHECK,    QHeaderView::ResizeToContents);
    mTable->horizontalHeader()->setSectionResizeMode(COL_CATEGORY, QHeaderView::ResizeToContents);
    mTable->horizontalHeader()->setSectionResizeMode(COL_PATH,     QHeaderView::Stretch);
    mTable->horizontalHeader()->setSectionResizeMode(COL_SIZE,     QHeaderView::ResizeToContents);
    mTable->horizontalHeader()->setSectionResizeMode(COL_SIGNALS,  QHeaderView::Stretch);
    mTable->verticalHeader()->setVisible(false);
    mTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    mTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    connect(mTable, &QTableWidget::itemChanged, this, &OrphanLeftoversDialog::onTableItemChanged);
    root->addWidget(mTable, 1);

    auto *buttons = new QHBoxLayout();
    buttons->addStretch();

    mBtnSkip = new QPushButton(tr("Skip"), this);
    mBtnSkip->setCursor(Qt::PointingHandCursor);
    connect(mBtnSkip, &QPushButton::clicked, this, &QDialog::reject);
    buttons->addWidget(mBtnSkip);

    // CISO §1: destructive action uses the red/danger accent
    // (QSS: QPushButton[accessibleName="danger"]). Disabled until at least
    // one item is checked.
    mBtnTrash = new QPushButton(tr("Move to Trash"), this);
    mBtnTrash->setAccessibleName("danger");
    mBtnTrash->setCursor(Qt::PointingHandCursor);
    mBtnTrash->setEnabled(false);
    connect(mBtnTrash, &QPushButton::clicked, this, &OrphanLeftoversDialog::onMoveToTrash);
    buttons->addWidget(mBtnTrash);

    root->addLayout(buttons);

    mLblSummary->setText(tr("Scanning for orphan leftovers…"));
    mTable->setEnabled(false);
}

void OrphanLeftoversDialog::scan()
{
    mTable->setEnabled(false);
    mBtnTrash->setEnabled(false);
    mLblSummary->setText(tr("Scanning for orphan leftovers…"));

    // SSO-15429 / SSO-3362-class worker-lifetime contract: findOrphanLeftovers()
    // walks the user's home directory off the UI thread, so the user can
    // close (WA_DeleteOnClose) the dialog before it finishes. Capture a
    // QPointer, not raw `this`, and re-check it after the scan and again
    // inside the marshaled slot; the destructor's waitForFinished() on
    // mScanFuture is the backstop if a check is ever missed.
    QPointer<OrphanLeftoversDialog> self(this);
    mScanFuture = QtConcurrent::run([self]() {
        const QList<OrphanLeftover> items = PackageService::ins()->findOrphanLeftovers();
        if (!self)
            return;
        QMetaObject::invokeMethod(self.data(), [self, items]() {
            if (!self)
                return;
            self->mTable->setEnabled(true);
            self->populate(items);
        }, Qt::QueuedConnection);
    });
}

void OrphanLeftoversDialog::populate(const QList<OrphanLeftover> &items)
{
    const QList<Row> rows = buildRows(items);

    mTable->blockSignals(true);
    mTotalBytes = 0;
    mItemCount  = rows.size();
    mTable->setRowCount(mItemCount);

    for (int row = 0; row < rows.size(); ++row) {
        const Row &r = rows.at(row);
        mTotalBytes += static_cast<qint64>(r.size);

        auto *chkItem = new QTableWidgetItem();
        chkItem->setFlags(chkItem->flags() | Qt::ItemIsUserCheckable);
        // CISO §5: default unchecked — explicit per-item review required.
        chkItem->setCheckState(r.checked ? Qt::Checked : Qt::Unchecked);
        chkItem->setData(ROLE_BYTES, QVariant::fromValue(r.size));
        mTable->setItem(row, COL_CHECK, chkItem);

        mTable->setItem(row, COL_CATEGORY, new QTableWidgetItem(r.category));

        auto *pathItem = new QTableWidgetItem(r.path);
        pathItem->setToolTip(r.path);
        mTable->setItem(row, COL_PATH, pathItem);

        auto *sizeItem = new QTableWidgetItem(FormatUtil::formatBytes(r.size));
        sizeItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        mTable->setItem(row, COL_SIZE, sizeItem);

        // CISO Psychological Acceptability: the full matched-signal set,
        // not just a confidence number — the reviewing user must see *why*
        // each item was flagged.
        const QString signalText = r.signalLabels.join(QStringLiteral(", "));
        auto *signalsItem = new QTableWidgetItem(signalText);
        signalsItem->setToolTip(signalText);
        mTable->setItem(row, COL_SIGNALS, signalsItem);
    }
    mTable->blockSignals(false);

    updateSummary();
}

QList<OrphanLeftoversDialog::Row> OrphanLeftoversDialog::buildRows(const QList<OrphanLeftover> &items)
{
    QList<Row> rows;
    rows.reserve(items.size());
    for (const OrphanLeftover &item : items) {
        Row row;
        row.path = item.path;
        row.category = item.category;
        row.size = item.size;
        for (const OrphanSignal &signal : item.matchedSignals)
            row.signalLabels.append(signal.humanLabel);
        // CISO §5 (SSO-15429): every orphan match starts unchecked — never
        // derived from confidenceScore or any other heuristic.
        row.checked = false;
        rows.append(row);
    }
    return rows;
}

QString OrphanLeftoversDialog::confirmationSentence(int itemCount, quint64 totalBytes)
{
    // Design Anchor (SSO-1768): one sentence maximum, size + count, no
    // permanent-delete wording — orphan scanner is trash-only.
    return QObject::tr("Move %1 item(s) (%2) to Trash?")
        .arg(itemCount)
        .arg(FormatUtil::formatBytes(totalBytes));
}

void OrphanLeftoversDialog::updateSummary()
{
    int checkedCount = 0;
    qint64 checkedBytes = 0;
    for (int row = 0; row < mTable->rowCount(); ++row) {
        auto *item = mTable->item(row, COL_CHECK);
        if (item && item->checkState() == Qt::Checked) {
            ++checkedCount;
            checkedBytes += static_cast<qint64>(item->data(ROLE_BYTES).toULongLong());
        }
    }

    if (mItemCount == 0) {
        mLblSummary->setText(tr("No orphan leftovers found."));
        mBtnTrash->setEnabled(false);
        return;
    }

    // CISO §1: one sentence max showing size + item count before any
    // destructive action.
    mLblSummary->setText(
        tr("Found %1 orphan leftover(s) totalling %2 — %3 selected (%4).")
            .arg(mItemCount)
            .arg(FormatUtil::formatBytes(static_cast<quint64>(mTotalBytes)))
            .arg(checkedCount)
            .arg(FormatUtil::formatBytes(static_cast<quint64>(checkedBytes))));

    mBtnTrash->setEnabled(checkedCount > 0);
}

void OrphanLeftoversDialog::onTableItemChanged()
{
    updateSummary();
}

QStringList OrphanLeftoversDialog::checkedPaths() const
{
    QStringList paths;
    for (int row = 0; row < mTable->rowCount(); ++row) {
        auto *chk = mTable->item(row, COL_CHECK);
        if (!chk || chk->checkState() != Qt::Checked)
            continue;
        auto *pathItem = mTable->item(row, COL_PATH);
        if (pathItem && !pathItem->text().isEmpty())
            paths.append(pathItem->text());
    }
    return paths;
}

void OrphanLeftoversDialog::onMoveToTrash()
{
    const QStringList paths = checkedPaths();
    if (paths.isEmpty())
        return;

    int checkedCount = 0;
    qint64 checkedBytes = 0;
    for (int row = 0; row < mTable->rowCount(); ++row) {
        auto *item = mTable->item(row, COL_CHECK);
        if (item && item->checkState() == Qt::Checked) {
            ++checkedCount;
            checkedBytes += static_cast<qint64>(item->data(ROLE_BYTES).toULongLong());
        }
    }

    // Design Anchor (SSO-1768): a distinct one-sentence confirmation step
    // ahead of the trash call — orphan matches are higher risk than a
    // post-uninstall leftover scan, so the summary label above is not
    // treated as confirmation on its own.
    const QString sentence = confirmationSentence(checkedCount, static_cast<quint64>(checkedBytes));
    const auto choice = QMessageBox::question(this, tr("Move to Trash"), sentence,
                                               QMessageBox::Yes | QMessageBox::Cancel,
                                               QMessageBox::Cancel);
    if (choice != QMessageBox::Yes)
        return;

    // PackageService::trashLeftovers() -> PackageTool::trashLeftovers():
    // deny-list check, CISO §3 audit log entry per item, QFile::moveToTrash
    // (freedesktop.org Trash spec on Linux) — fail-secure, never a
    // permanent-delete fallback.
    const bool allOk = PackageService::ins()->trashLeftovers(paths);
    if (allOk) {
        accept();
        return;
    }

    // Fail-secure: a failed item is aborted (left in place), never
    // permanently deleted. Surface that to the user instead of silently
    // reporting success, and re-scan so the table reflects what actually
    // happened — trashed items disappear, failed items remain visible.
    QMessageBox::warning(this, tr("Move to Trash"),
                          tr("Some items could not be moved to Trash and remain listed below."));
    scan();
}
