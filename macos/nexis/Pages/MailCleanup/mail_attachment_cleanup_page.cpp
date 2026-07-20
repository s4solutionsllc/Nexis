#include "mail_attachment_cleanup_page.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QProgressBar>
#include <QTableWidget>
#include <QHeaderView>
#include <QFrame>
#include <QDialog>
#include <QDialogButtonBox>
#include <QMessageBox>
#include <QtConcurrent>
#include <QLocale>
#include <QCheckBox>

// ─────────────────────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────────────────────

static QString fmtSize(qint64 bytes)
{
    return QLocale().formattedDataSize(bytes, 2, QLocale::DataSizeTraditionalFormat);
}

// ─────────────────────────────────────────────────────────────────────────────
// ctor / UI build
// ─────────────────────────────────────────────────────────────────────────────

MailAttachmentCleanupPage::MailAttachmentCleanupPage(QWidget *parent)
    : QWidget(parent)
    , mTool(new MailAttachmentTool(this))
{
    buildUI();
    setPhase(0);

    connect(mTool, &MailAttachmentTool::scanFinished,
            this, &MailAttachmentCleanupPage::onScanFinished, Qt::QueuedConnection);
    connect(mTool, &MailAttachmentTool::progress,
            this, &MailAttachmentCleanupPage::onProgress, Qt::QueuedConnection);
    connect(mTool, &MailAttachmentTool::deleteFinished,
            this, &MailAttachmentCleanupPage::onDeleteFinished, Qt::QueuedConnection);
}

void MailAttachmentCleanupPage::buildUI()
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(24, 24, 24, 24);
    root->setSpacing(16);

    // ── Title ──
    mLblTitle = new QLabel(tr("Mail Attachment Cleanup"), this);
    mLblTitle->setObjectName("pageTitle");
    root->addWidget(mLblTitle);

    // ── Description ──
    mLblDescription = new QLabel(
        tr("Nexis can remove locally stored Mail attachments from "
           "~/Library/Mail to recover disk space. Attachments deleted here "
           "are removed from Mail's local store only — for IMAP accounts they "
           "can typically be re-downloaded from the mail server on next sync, "
           "but attachments from POP or locally-only accounts may not be "
           "recoverable."),
        this);
    mLblDescription->setWordWrap(true);
    mLblDescription->setObjectName("descriptionLabel");
    root->addWidget(mLblDescription);

    // ── Mail-running warning (hidden until scan detects it) ──
    mLblMailWarning = new QLabel(
        tr("Mail is currently open. Close Mail before deleting attachments to "
           "avoid mailbox index corruption."),
        this);
    mLblMailWarning->setWordWrap(true);
    mLblMailWarning->setObjectName("warningBanner");
    mLblMailWarning->setVisible(false);
    root->addWidget(mLblMailWarning);

    // ── Scan row ──
    auto *scanRow = new QHBoxLayout();
    mBtnScan = new QPushButton(tr("Scan for Attachments"), this);
    mBtnScan->setObjectName("primaryButton");
    scanRow->addWidget(mBtnScan);
    mLblScanStatus = new QLabel(this);
    mLblScanStatus->setVisible(false);
    scanRow->addWidget(mLblScanStatus, 1);
    root->addLayout(scanRow);

    // ── Indeterminate scan progress ──
    mScanProgress = new QProgressBar(this);
    mScanProgress->setRange(0, 0);
    mScanProgress->setVisible(false);
    mScanProgress->setFixedHeight(4);
    root->addWidget(mScanProgress);

    // ── Results table ──
    mTable = new QTableWidget(0, 5, this);
    mTable->setHorizontalHeaderLabels({
        QString(), // checkbox column
        tr("Filename"),
        tr("Size"),
        tr("Date"),
        tr("Sender / Subject")
    });
    mTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    mTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    mTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    mTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    mTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);
    mTable->setColumnWidth(0, 32);
    mTable->setSelectionMode(QAbstractItemView::NoSelection);
    mTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    mTable->setAlternatingRowColors(true);
    mTable->setVisible(false);
    root->addWidget(mTable, 1);

    // ── Selection row ──
    auto *selectRow = new QHBoxLayout();
    mBtnSelectAll = new QPushButton(tr("Select All"), this);
    mBtnSelectAll->setVisible(false);
    selectRow->addWidget(mBtnSelectAll);
    mLblSummary = new QLabel(this);
    mLblSummary->setVisible(false);
    selectRow->addWidget(mLblSummary, 1);
    root->addLayout(selectRow);

    // ── Confirmation frame + delete button ──
    mConfirmFrame = new QFrame(this);
    mConfirmFrame->setObjectName("confirmFrame");
    mConfirmFrame->setFrameShape(QFrame::StyledPanel);
    auto *cfLayout = new QVBoxLayout(mConfirmFrame);
    cfLayout->setContentsMargins(12, 12, 12, 12);
    cfLayout->setSpacing(8);

    auto *riskLabel = new QLabel(
        tr("Deleting attachments is permanent for this device. IMAP accounts "
           "can usually re-download from the server; POP and local-only "
           "accounts cannot."),
        mConfirmFrame);
    riskLabel->setWordWrap(true);
    riskLabel->setObjectName("riskNote");
    cfLayout->addWidget(riskLabel);

    mBtnDelete = new QPushButton(tr("Delete Selected Attachments"), mConfirmFrame);
    mBtnDelete->setObjectName("destructiveButton");
    mBtnDelete->setEnabled(false);
    cfLayout->addWidget(mBtnDelete, 0, Qt::AlignRight);

    mConfirmFrame->setVisible(false);
    root->addWidget(mConfirmFrame);

    // ── Delete progress ──
    mDeleteProgress = new QProgressBar(this);
    mDeleteProgress->setRange(0, 100);
    mDeleteProgress->setVisible(false);
    mDeleteProgress->setFixedHeight(4);
    root->addWidget(mDeleteProgress);

    // ── Result label ──
    mLblResult = new QLabel(this);
    mLblResult->setVisible(false);
    root->addWidget(mLblResult);

    root->addStretch();

    // ── Connections ──
    connect(mBtnScan,      &QPushButton::clicked, this, &MailAttachmentCleanupPage::onScanClicked);
    connect(mBtnDelete,    &QPushButton::clicked, this, &MailAttachmentCleanupPage::onDeleteClicked);
    connect(mBtnSelectAll, &QPushButton::clicked, this, &MailAttachmentCleanupPage::onSelectAll);
}

// ─────────────────────────────────────────────────────────────────────────────
// Phase management
// ─────────────────────────────────────────────────────────────────────────────

void MailAttachmentCleanupPage::setPhase(int phase)
{
    // phase 0 = idle, 1 = scanning, 2 = preview, 3 = deleting, 4 = done
    mBtnScan->setVisible(phase == 0 || phase == 2);
    mBtnScan->setEnabled(phase == 0 || phase == 2);
    mScanProgress->setVisible(phase == 1);
    mLblScanStatus->setVisible(phase == 1 || phase == 2);
    mTable->setVisible(phase == 2 || phase == 3 || phase == 4);
    mBtnSelectAll->setVisible(phase == 2);
    mLblSummary->setVisible(phase == 2 || phase == 3);
    mConfirmFrame->setVisible(phase == 2);
    mDeleteProgress->setVisible(phase == 3);
    mLblResult->setVisible(phase == 4);
}

// ─────────────────────────────────────────────────────────────────────────────
// Scan
// ─────────────────────────────────────────────────────────────────────────────

void MailAttachmentCleanupPage::onScanClicked()
{
    if (mScanInProgress || mDeleteInProgress)
        return;

    mScanInProgress = true;
    mLblMailWarning->setVisible(false);
    mLblResult->setVisible(false);
    mTable->setRowCount(0);
    mEntries.clear();
    setPhase(1);
    mLblScanStatus->setText(tr("Scanning…"));

    mWorkerFuture = QtConcurrent::run([this]() {
        mTool->scan();
    });
}

void MailAttachmentCleanupPage::onScanFinished(const MailAttachmentScanResult &result)
{
    mScanInProgress = false;

    if (result.mailRunning)
        mLblMailWarning->setVisible(true);

    mEntries = result.entries;

    if (result.entries.isEmpty()) {
        mLblScanStatus->setText(tr("No Mail attachments found."));
        setPhase(0);
        return;
    }

    mLblScanStatus->setText(
        tr("Found %1 attachment%2 (%3 total).")
            .arg(result.entries.size())
            .arg(result.entries.size() == 1 ? QString() : QStringLiteral("s"))
            .arg(fmtSize(result.totalSize)));

    populateTable(result.entries);
    updateSummaryLabel();
    setPhase(2);
}

// ─────────────────────────────────────────────────────────────────────────────
// Table population
// ─────────────────────────────────────────────────────────────────────────────

void MailAttachmentCleanupPage::populateTable(const QList<MailAttachmentEntry> &entries)
{
    mTable->setRowCount(0);
    mTable->setRowCount(entries.size());

    for (int i = 0; i < entries.size(); ++i) {
        const auto &e = entries.at(i);

        // Column 0 — checkbox
        auto *chk = new QCheckBox(this);
        chk->setChecked(true);
        connect(chk, &QCheckBox::checkStateChanged, this, [this](Qt::CheckState) {
            onSelectionChanged();
        });
        auto *cellWidget = new QWidget(this);
        auto *cellLayout = new QHBoxLayout(cellWidget);
        cellLayout->setContentsMargins(4, 0, 4, 0);
        cellLayout->addWidget(chk);
        mTable->setCellWidget(i, 0, cellWidget);

        // Column 1 — filename
        auto *fnItem = new QTableWidgetItem(e.filename);
        fnItem->setData(Qt::UserRole, e.path);  // store path for retrieval
        mTable->setItem(i, 1, fnItem);

        // Column 2 — size
        mTable->setItem(i, 2, new QTableWidgetItem(fmtSize(e.size)));

        // Column 3 — date (originating message date; falls back to file mtime
        // when the .emlx envelope has no parseable Date: header)
        const QDateTime &displayDate = e.messageDate.isValid() ? e.messageDate : e.modified;
        const QString dateStr = displayDate.isValid()
            ? displayDate.toLocalTime().toString(QStringLiteral("yyyy-MM-dd"))
            : QString();
        mTable->setItem(i, 3, new QTableWidgetItem(dateStr));

        // Column 4 — sender / subject
        QString who;
        if (!e.sender.isEmpty())
            who = e.sender;
        if (!e.subject.isEmpty())
            who += (who.isEmpty() ? QString() : QStringLiteral(" — ")) + e.subject;
        mTable->setItem(i, 4, new QTableWidgetItem(who));
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Selection helpers
// ─────────────────────────────────────────────────────────────────────────────

QStringList MailAttachmentCleanupPage::selectedPaths() const
{
    QStringList paths;
    for (int i = 0; i < mTable->rowCount(); ++i) {
        auto *cellW = mTable->cellWidget(i, 0);
        auto *chk = cellW ? cellW->findChild<QCheckBox*>() : nullptr;
        if (chk && chk->isChecked()) {
            auto *fnItem = mTable->item(i, 1);
            if (fnItem)
                paths << fnItem->data(Qt::UserRole).toString();
        }
    }
    return paths;
}

void MailAttachmentCleanupPage::onSelectAll()
{
    for (int i = 0; i < mTable->rowCount(); ++i) {
        auto *cellW = mTable->cellWidget(i, 0);
        auto *chk = cellW ? cellW->findChild<QCheckBox*>() : nullptr;
        if (chk) chk->setChecked(true);
    }
    updateSummaryLabel();
}

void MailAttachmentCleanupPage::onSelectionChanged()
{
    updateSummaryLabel();
}

void MailAttachmentCleanupPage::updateSummaryLabel()
{
    qint64 selSize = 0;
    int    selCount = 0;
    for (int i = 0; i < mTable->rowCount(); ++i) {
        auto *cellW = mTable->cellWidget(i, 0);
        auto *chk = cellW ? cellW->findChild<QCheckBox*>() : nullptr;
        if (chk && chk->isChecked()) {
            ++selCount;
            // Lookup size from mEntries by row index
            if (i < mEntries.size())
                selSize += mEntries.at(i).size;
        }
    }

    mLblSummary->setText(
        tr("%1 item%2 selected · %3")
            .arg(selCount)
            .arg(selCount == 1 ? QString() : QStringLiteral("s"))
            .arg(fmtSize(selSize)));

    mBtnDelete->setEnabled(selCount > 0);
}

// ─────────────────────────────────────────────────────────────────────────────
// Delete
// ─────────────────────────────────────────────────────────────────────────────

void MailAttachmentCleanupPage::onDeleteClicked()
{
    if (mDeleteInProgress || mScanInProgress)
        return;

    const QStringList paths = selectedPaths();
    if (paths.isEmpty())
        return;

    // Final confirmation dialog — one-sentence body, destructive accent.
    QMessageBox confirm(this);
    confirm.setWindowTitle(tr("Delete Mail Attachments"));
    confirm.setText(
        tr("Nexis will permanently remove %1 attachment%2 (%3) from this "
           "device's Mail store — typically re-downloadable from the server "
           "for IMAP accounts, but not recoverable for POP or local-only accounts.")
            .arg(paths.size())
            .arg(paths.size() == 1 ? QString() : QStringLiteral("s"))
            .arg(fmtSize([&]() {
                qint64 s = 0;
                for (int i = 0; i < mTable->rowCount(); ++i) {
                    auto *cw = mTable->cellWidget(i, 0);
                    auto *chk = cw ? cw->findChild<QCheckBox*>() : nullptr;
                    if (chk && chk->isChecked() && i < mEntries.size())
                        s += mEntries.at(i).size;
                }
                return s;
            }())));
    auto *del = confirm.addButton(tr("Delete Attachments"), QMessageBox::DestructiveRole);
    confirm.addButton(tr("Cancel"), QMessageBox::RejectRole);
    confirm.setDefaultButton(del);
    confirm.exec();

    if (confirm.clickedButton() != del)
        return;

    mDeleteInProgress = true;
    setPhase(3);
    mDeleteProgress->setValue(0);

    const QStringList pathsCopy = paths;
    mWorkerFuture = QtConcurrent::run([this, pathsCopy]() {
        mTool->deleteAttachments(pathsCopy);
    });
}

void MailAttachmentCleanupPage::onProgress(int done, int total, const QString & /*currentFile*/)
{
    if (total > 0) {
        mDeleteProgress->setRange(0, total);
        mDeleteProgress->setValue(done);
    }
}

void MailAttachmentCleanupPage::onDeleteFinished(qint64 freedBytes, int deletedCount, int failedCount)
{
    mDeleteInProgress = false;
    setPhase(4);

    QString msg = tr("Deleted %1 attachment%2, freed %3.")
        .arg(deletedCount)
        .arg(deletedCount == 1 ? QString() : QStringLiteral("s"))
        .arg(fmtSize(freedBytes));

    if (failedCount > 0)
        msg += QStringLiteral(" ") + tr("%1 item%2 could not be removed.")
            .arg(failedCount)
            .arg(failedCount == 1 ? QString() : QStringLiteral("s"));

    mLblResult->setText(msg);

    // Offer another scan
    mBtnScan->setVisible(true);
    mBtnScan->setText(tr("Scan Again"));
    setPhase(0);
    mLblResult->setVisible(true);
}
