#include "trust_safety_preview_dialog.h"

#include "dpi.h"
#include "signal_mapper.h"
#include <Managers/app_manager.h>
#include <Utils/format_util.h>

#include <QBrush>
#include <QCheckBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QSettings>
#include <QStyle>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace {

// Design Anchor (SSO-1785): "Confirmation dialog copy: one sentence
// maximum." Heuristic check — counts sentence terminators that aren't part
// of a decimal number (so "1.5 GB will be freed." still reads as one
// sentence). Debug-only guard; adopters are expected to write real
// one-sentence copy, this just catches an obvious miss at integration time.
bool looksLikeSingleSentence(const QString &text)
{
    int terminators = 0;
    for (int i = 0; i < text.length(); ++i) {
        const QChar c = text.at(i);
        if (c == QLatin1Char('.') || c == QLatin1Char('!') || c == QLatin1Char('?')) {
            const bool afterDigit = i > 0 && text.at(i - 1).isDigit();
            const bool beforeDigit = i + 1 < text.length() && text.at(i + 1).isDigit();
            if (!(afterDigit && beforeDigit))
                terminators++;
        }
    }
    return terminators <= 1;
}

} // namespace

TrustSafetyPreviewDialog::TrustSafetyPreviewDialog(TrustSafetyActionProvider *provider,
                                                     Config config,
                                                     QWidget *parent,
                                                     AppManager *appManager)
    : QDialog(parent),
      mProvider(provider),
      mConfig(std::move(config)),
      mAppManager(appManager ? appManager : AppManager::ins())
{
    Q_ASSERT_X(looksLikeSingleSentence(mConfig.confirmationSentence), "TrustSafetyPreviewDialog",
               "Design Anchor (SSO-1785): confirmation copy must be one sentence maximum.");

    mRunner = new TrustSafetyRunner(mProvider, this);
    connect(mRunner, &TrustSafetyRunner::itemDiscovered, this, &TrustSafetyPreviewDialog::onItemDiscovered);
    connect(mRunner, &TrustSafetyRunner::scanFinished, this, &TrustSafetyPreviewDialog::onScanFinished);
    connect(mRunner, &TrustSafetyRunner::scanCancelled, this, &TrustSafetyPreviewDialog::onScanCancelled);
    connect(mRunner, &TrustSafetyRunner::executionProgress, this, &TrustSafetyPreviewDialog::onExecutionProgress);
    connect(mRunner, &TrustSafetyRunner::executionFinished, this, &TrustSafetyPreviewDialog::onExecutionFinished);

    buildUI();
    setWindowTitle(mConfig.windowTitle);

    connect(SignalMapper::ins(), &SignalMapper::sigChangedAppTheme,
            this, &TrustSafetyPreviewDialog::refreshThemeColors);
    refreshThemeColors();

    mProgressBar->setRange(0, 0); // indeterminate — total item count is unknown until the scan finishes
    mRunner->startScan();
    updateControlsForState();
}

TrustSafetyPreviewDialog::~TrustSafetyPreviewDialog() = default;

void TrustSafetyPreviewDialog::buildUI()
{
    setObjectName("trustSafetyPreviewDialog");
    setMinimumSize(Dpi::scale(680, 480));

    QVBoxLayout *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // Design Anchor: progress bar thin (4-6px), anchored to the dialog's
    // header — placed before any other content, full width.
    mProgressBar = new QProgressBar(this);
    mProgressBar->setObjectName("trustSafetyProgress");
    mProgressBar->setProperty("progressRole", "thin");
    mProgressBar->setTextVisible(false);
    mProgressBar->setFixedHeight(Dpi::scale(5));
    root->addWidget(mProgressBar);

    QVBoxLayout *content = new QVBoxLayout;
    content->setContentsMargins(Dpi::scale(20), Dpi::scale(15), Dpi::scale(20), Dpi::scale(15));
    content->setSpacing(Dpi::scale(10));
    root->addLayout(content, 1);

    mLblTitle = new QLabel(mConfig.windowTitle, this);
    mLblTitle->setProperty("accessibleName", "dialog-title");
    content->addWidget(mLblTitle);

    mLblScanStatus = new QLabel(tr("Scanning…"), this);
    content->addWidget(mLblScanStatus);

    QHBoxLayout *toolRow = new QHBoxLayout;
    mChkSelectAll = new QCheckBox(tr("Select All"), this);
    mChkDryRun = new QCheckBox(tr("Dry run (preview only, no changes)"), this);
    toolRow->addWidget(mChkSelectAll);
    toolRow->addStretch();
    toolRow->addWidget(mChkDryRun);
    content->addLayout(toolRow);

    // Item column: checkbox + label. Description column: the plain-English
    // explain-before-run sentence, always visible (not buried in a tooltip
    // or a modal). Command column: the exact underlying operation, also
    // always visible, monospaced. Both are additionally set as tooltips in
    // addOrUpdateItemRow() in case the columns get narrowed/truncated.
    mTree = new QTreeWidget(this);
    mTree->setColumnCount(4);
    mTree->setHeaderLabels({tr("Item"), tr("What / why"), tr("Command"), tr("Size")});
    mTree->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    mTree->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    mTree->header()->setSectionResizeMode(2, QHeaderView::Interactive);
    mTree->header()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    mTree->header()->resizeSection(2, Dpi::scale(240));
    mTree->setRootIsDecorated(true);
    mTree->setUniformRowHeights(true);
    mTree->setAlternatingRowColors(true);
    content->addWidget(mTree, 1);

    // Matches the "JetBrains Mono" family used elsewhere for command/path
    // cells (e.g. Processes page's cmd column) — registered as an
    // application font in main.cpp.
    mMonoFont = QFont(QStringLiteral("JetBrains Mono"));

    // Design Anchor: status bar persistent, shows total selected size +
    // item count, updates live as selection changes.
    QHBoxLayout *statusRow = new QHBoxLayout;
    mLblStatusBar = new QLabel(tr("0 selected · %1").arg(FormatUtil::formatBytes(0)), this);
    mLblStatusBar->setObjectName("trustSafetyStatusBar");
    statusRow->addWidget(mLblStatusBar);
    statusRow->addStretch();
    content->addLayout(statusRow);

    // Design Anchor: primary action always visible, disabled (never a
    // silent no-op) until at least one item is checked. Destructive primary
    // = red accent ("danger"); secondary/cancel = default outlined style.
    QHBoxLayout *btnRow = new QHBoxLayout;
    mBtnCancel = new QPushButton(tr("Close"), this);
    mBtnPrimary = new QPushButton(mConfig.primaryActionLabel, this);
    mBtnPrimary->setProperty("accessibleName", "danger");
    mBtnPrimary->setEnabled(false);
    btnRow->addWidget(mBtnCancel);
    btnRow->addStretch();
    btnRow->addWidget(mBtnPrimary);
    content->addLayout(btnRow);

    connect(mChkSelectAll, &QCheckBox::toggled, this, &TrustSafetyPreviewDialog::onSelectAllToggled);
    connect(mChkDryRun, &QCheckBox::toggled, this, &TrustSafetyPreviewDialog::onDryRunToggled);
    connect(mTree, &QTreeWidget::itemChanged, this, &TrustSafetyPreviewDialog::onItemCheckChanged);
    connect(mBtnPrimary, &QPushButton::clicked, this, &TrustSafetyPreviewDialog::onPrimaryActionClicked);
    connect(mBtnCancel, &QPushButton::clicked, this, &TrustSafetyPreviewDialog::onCancelClicked);
}

void TrustSafetyPreviewDialog::refreshThemeColors()
{
    QSettings *sv = mAppManager->getStyleValues();
    if (sv)
        mWarningColor = QColor(sv->value("@warningColor", "#FFB347").toString());
    applyRiskStyling();
}

void TrustSafetyPreviewDialog::applyRiskStyling()
{
    for (auto it = mCategoryRows.constBegin(); it != mCategoryRows.constEnd(); ++it)
        updateCategorySummary(it.value());

    for (auto it = mItemRows.constBegin(); it != mItemRows.constEnd(); ++it) {
        const TrustSafetyActionItem &item = mItemsById.value(it.key());
        if (item.riskTier == TrustSafetyActionItem::RiskTier::Risky)
            it.value()->setForeground(0, QBrush(mWarningColor));
    }
}

// ─── Scan phase ────────────────────────────────────────────────────────────

void TrustSafetyPreviewDialog::onItemDiscovered(TrustSafetyActionItem item)
{
    mItemsById.insert(item.id, item);
    addOrUpdateItemRow(item);
}

void TrustSafetyPreviewDialog::onScanFinished(QList<TrustSafetyActionItem> items)
{
    Q_UNUSED(items);
    mLblScanStatus->setText(tr("Scan complete — %1 item(s) found.").arg(mItemsById.size()));
    updateControlsForState();
}

void TrustSafetyPreviewDialog::onScanCancelled()
{
    mLblScanStatus->setText(tr("Scan cancelled — showing %1 item(s) found so far.").arg(mItemsById.size()));
    updateControlsForState();
}

// ─── Execution phase ───────────────────────────────────────────────────────

void TrustSafetyPreviewDialog::onExecutionProgress(int itemsDone, int itemsTotal, qint64 bytesFreedSoFar)
{
    mProgressBar->setRange(0, itemsTotal);
    mProgressBar->setValue(itemsDone);
    const quint64 freedSoFar = static_cast<quint64>(qMax<qint64>(0, bytesFreedSoFar));
    mLblScanStatus->setText(tr("Cleaning… %1 of %2 (%3 freed so far).")
        .arg(itemsDone).arg(itemsTotal).arg(FormatUtil::formatBytes(freedSoFar)));
}

void TrustSafetyPreviewDialog::onExecutionFinished(TrustSafetyRunSummary summary)
{
    mLastSummary = summary;

    const quint64 freed = static_cast<quint64>(qMax<qint64>(0, summary.totalBytesFreed));
    if (summary.cancelled) {
        mLblScanStatus->setText(tr("Stopped after %1 of %2 item(s) — %3 freed so far.")
            .arg(summary.results.size()).arg(summary.totalItemsRequested)
            .arg(FormatUtil::formatBytes(freed)));
    } else if (summary.dryRun) {
        mLblScanStatus->setText(tr("Dry run complete — would free %1 from %2 of %3 item(s).")
            .arg(FormatUtil::formatBytes(freed))
            .arg(summary.totalItemsSucceeded).arg(summary.totalItemsRequested));
    } else {
        mLblScanStatus->setText(tr("Done — freed %1 from %2 of %3 item(s).")
            .arg(FormatUtil::formatBytes(freed))
            .arg(summary.totalItemsSucceeded).arg(summary.totalItemsRequested));
    }

    if (!summary.dryRun) {
        // Items that were actually removed no longer exist — drop them from
        // the preview instead of leaving a stale checked row behind. Items
        // that failed (or weren't reached because Stop was hit) stay, so
        // partial completion is visible and the user can retry them.
        for (const TrustSafetyActionResult &result : summary.results) {
            if (!result.succeeded)
                continue;
            QTreeWidgetItem *row = mItemRows.take(result.itemId);
            if (!row)
                continue;
            QTreeWidgetItem *category = row->parent();
            delete row;
            mItemsById.remove(result.itemId);
            if (category)
                updateCategorySummary(category);
        }
    }

    updateControlsForState();
}

// ─── User actions ──────────────────────────────────────────────────────────

void TrustSafetyPreviewDialog::onPrimaryActionClicked()
{
    const QList<TrustSafetyActionItem> selected = checkedItems();
    if (selected.isEmpty())
        return; // primary is disabled in this state — defensive only

    const bool dryRun = mChkDryRun->isChecked();

    if (!dryRun) {
        qint64 totalSize = 0;
        for (const TrustSafetyActionItem &item : selected)
            totalSize += qMax<qint64>(0, item.estimatedSizeBytes);

        // Design Anchor: every destructive-action confirmation shows a
        // "what will be deleted" summary (total size + item count) before
        // the user can confirm; primary copy stays to one sentence.
        QMessageBox box(this);
        box.setIcon(QMessageBox::Warning);
        box.setWindowTitle(mConfig.windowTitle);
        box.setText(mConfig.confirmationSentence);
        box.setInformativeText(tr("%1 item(s) · %2")
            .arg(selected.size())
            .arg(FormatUtil::formatBytes(static_cast<quint64>(qMax<qint64>(0, totalSize)))));
        box.setStandardButtons(QMessageBox::Cancel | QMessageBox::Ok);
        box.setDefaultButton(QMessageBox::Cancel);
        box.button(QMessageBox::Ok)->setText(mConfig.primaryActionLabel);
        if (box.exec() != QMessageBox::Ok)
            return;
    }

    mProgressBar->setRange(0, selected.size());
    mProgressBar->setValue(0);
    mRunner->startExecution(selected, dryRun);
    updateControlsForState();
}

void TrustSafetyPreviewDialog::onCancelClicked()
{
    // Design Anchor + acceptance criteria: Cancel/Stop must actually halt
    // the in-flight operation, not just dismiss the dialog.
    if (mRunner->isExecuting()) {
        mRunner->cancelExecution();
        mBtnCancel->setEnabled(false); // avoid double-cancel while the worker unwinds
    } else if (mRunner->isScanning()) {
        mRunner->cancelScan();
        mBtnCancel->setEnabled(false);
    } else {
        reject();
    }
}

void TrustSafetyPreviewDialog::onDryRunToggled(bool checked)
{
    // Only the toggle for a real run is destructive; dry-run's primary
    // action never touches the filesystem, so it gets the non-destructive
    // accent style instead of the red "danger" style.
    mBtnPrimary->setText(checked ? tr("%1 (Dry Run)").arg(mConfig.primaryActionLabel) : mConfig.primaryActionLabel);
    mBtnPrimary->setProperty("accessibleName", checked ? "primary" : "danger");
    mBtnPrimary->style()->unpolish(mBtnPrimary);
    mBtnPrimary->style()->polish(mBtnPrimary);
}

void TrustSafetyPreviewDialog::onSelectAllToggled(bool checked)
{
    for (int i = 0; i < mTree->topLevelItemCount(); ++i)
        mTree->topLevelItem(i)->setCheckState(0, checked ? Qt::Checked : Qt::Unchecked);
}

void TrustSafetyPreviewDialog::onItemCheckChanged(QTreeWidgetItem *item, int column)
{
    if (mSuppressCheckSignal || column != 0)
        return;

    const bool isCategoryRow = (item->parent() == nullptr);

    if (isCategoryRow) {
        const QString categoryId = item->data(0, Qt::UserRole + 1).toString();
        const Qt::CheckState newState = item->checkState(0);

        if (newState == Qt::Checked && !mRiskyConfirmedCategoryIds.contains(categoryId)) {
            bool anyRisky = false;
            for (int i = 0; i < item->childCount() && !anyRisky; ++i) {
                const TrustSafetyActionItem &child =
                    mItemsById.value(item->child(i)->data(0, Qt::UserRole).toString());
                anyRisky = (child.riskTier == TrustSafetyActionItem::RiskTier::Risky);
            }
            if (anyRisky && !confirmRiskyCategory(categoryId)) {
                mSuppressCheckSignal = true;
                item->setCheckState(0, Qt::Unchecked);
                mSuppressCheckSignal = false;
                return;
            }
        }

        mSuppressCheckSignal = true;
        for (int i = 0; i < item->childCount(); ++i)
            item->child(i)->setCheckState(0, newState == Qt::Unchecked ? Qt::Unchecked : Qt::Checked);
        mSuppressCheckSignal = false;
    } else {
        QTreeWidgetItem *category = item->parent();
        if (item->checkState(0) == Qt::Checked) {
            const TrustSafetyActionItem &action =
                mItemsById.value(item->data(0, Qt::UserRole).toString());
            if (action.riskTier == TrustSafetyActionItem::RiskTier::Risky &&
                !mRiskyConfirmedCategoryIds.contains(action.categoryId) &&
                !confirmRiskyCategory(action.categoryId)) {
                mSuppressCheckSignal = true;
                item->setCheckState(0, Qt::Unchecked);
                mSuppressCheckSignal = false;
                return;
            }
        }

        if (category) {
            int checkedChildren = 0;
            for (int i = 0; i < category->childCount(); ++i)
                if (category->child(i)->checkState(0) == Qt::Checked)
                    checkedChildren++;

            mSuppressCheckSignal = true;
            if (checkedChildren == 0)
                category->setCheckState(0, Qt::Unchecked);
            else if (checkedChildren == category->childCount())
                category->setCheckState(0, Qt::Checked);
            else
                category->setCheckState(0, Qt::PartiallyChecked);
            mSuppressCheckSignal = false;
        }
    }

    updateStatusBar();
    updatePrimaryActionEnabled();
}

// ─── Helpers ────────────────────────────────────────────────────────────────

bool TrustSafetyPreviewDialog::confirmRiskyCategory(const QString &categoryId)
{
    const QString label = mCategoryLabels.value(categoryId, categoryId);

    // Design Anchor: risky categories get an explicit extra confirmation
    // step before their items can be checked; copy stays one sentence.
    QMessageBox box(this);
    box.setIcon(QMessageBox::Warning);
    box.setWindowTitle(tr("Higher-Risk Category"));
    box.setText(tr("“%1” includes higher-risk items — select anyway?").arg(label));
    box.setStandardButtons(QMessageBox::Cancel | QMessageBox::Ok);
    box.setDefaultButton(QMessageBox::Cancel);
    if (box.exec() != QMessageBox::Ok)
        return false;

    mRiskyConfirmedCategoryIds.insert(categoryId);
    return true;
}

QTreeWidgetItem *TrustSafetyPreviewDialog::categoryRow(const QString &categoryId, const QString &categoryLabel)
{
    QTreeWidgetItem *row = mCategoryRows.value(categoryId, nullptr);
    if (row)
        return row;

    row = new QTreeWidgetItem(mTree);
    row->setFlags(row->flags() | Qt::ItemIsUserCheckable | Qt::ItemIsAutoTristate);
    mSuppressCheckSignal = true;
    row->setCheckState(0, Qt::Unchecked);
    mSuppressCheckSignal = false;
    row->setExpanded(true);
    row->setData(0, Qt::UserRole + 1, categoryId);
    QFont f = row->font(0);
    f.setBold(true);
    row->setFont(0, f);

    mCategoryRows.insert(categoryId, row);
    mCategoryLabels.insert(categoryId, categoryLabel);
    return row;
}

void TrustSafetyPreviewDialog::addOrUpdateItemRow(const TrustSafetyActionItem &item)
{
    QTreeWidgetItem *category = categoryRow(item.categoryId, item.categoryLabel);

    QTreeWidgetItem *row = mItemRows.value(item.id, nullptr);
    if (!row) {
        row = new QTreeWidgetItem(category);
        row->setFlags((row->flags() | Qt::ItemIsUserCheckable) & ~Qt::ItemIsAutoTristate);
        mSuppressCheckSignal = true;
        row->setCheckState(0, Qt::Unchecked);
        mSuppressCheckSignal = false;
        row->setData(0, Qt::UserRole, item.id);
        mItemRows.insert(item.id, row);
    }

    row->setText(0, item.label);
    row->setText(1, item.description);
    row->setText(2, item.command);
    row->setFont(2, mMonoFont);
    row->setToolTip(0, item.description);
    row->setToolTip(2, item.command);
    row->setText(3, item.estimatedSizeBytes >= 0
        ? FormatUtil::formatBytes(static_cast<quint64>(item.estimatedSizeBytes))
        : tr("…"));
    row->setTextAlignment(3, Qt::AlignRight | Qt::AlignVCenter);
    if (item.riskTier == TrustSafetyActionItem::RiskTier::Risky)
        row->setForeground(0, QBrush(mWarningColor));

    updateCategorySummary(category);
}

void TrustSafetyPreviewDialog::updateCategorySummary(QTreeWidgetItem *category)
{
    if (!category)
        return;

    const QString categoryId = category->data(0, Qt::UserRole + 1).toString();
    const QString label = mCategoryLabels.value(categoryId, categoryId);

    qint64 total = 0;
    bool anyUnknown = false;
    bool anyRisky = false;
    for (int i = 0; i < category->childCount(); ++i) {
        const TrustSafetyActionItem &item =
            mItemsById.value(category->child(i)->data(0, Qt::UserRole).toString());
        if (item.estimatedSizeBytes >= 0)
            total += item.estimatedSizeBytes;
        else
            anyUnknown = true;
        if (item.riskTier == TrustSafetyActionItem::RiskTier::Risky)
            anyRisky = true;
    }

    const int count = category->childCount();
    const QString sizeText = FormatUtil::formatBytes(static_cast<quint64>(qMax<qint64>(0, total)));
    category->setText(0, anyRisky ? tr("%1 (%2) · Risky").arg(label).arg(count)
                                   : tr("%1 (%2)").arg(label).arg(count));
    category->setText(3, anyUnknown ? tr("%1+…").arg(sizeText) : sizeText);
    category->setTextAlignment(3, Qt::AlignRight | Qt::AlignVCenter);
    if (anyRisky)
        category->setForeground(0, QBrush(mWarningColor));
}

void TrustSafetyPreviewDialog::updateControlsForState()
{
    const bool scanning = mRunner->isScanning();
    const bool executing = mRunner->isExecuting();
    const bool busy = scanning || executing;

    // The tree stays interactive during scanning (items keep streaming in,
    // per Design Anchor "render sizes incrementally"); it locks only while
    // an execution is actually in flight so the selection can't shift
    // underneath a running operation. The primary action stays disabled
    // through the scan too — running against a still-growing item set would
    // be confusing — and re-enables the moment onScanFinished() fires.
    mTree->setEnabled(!executing);
    mChkSelectAll->setEnabled(!executing);
    mChkDryRun->setEnabled(!executing);
    mBtnCancel->setEnabled(true);
    mBtnCancel->setText(busy ? tr("Stop") : tr("Close"));
    mProgressBar->setVisible(busy);

    updatePrimaryActionEnabled();
}

void TrustSafetyPreviewDialog::updatePrimaryActionEnabled()
{
    const bool busy = mRunner->isScanning() || mRunner->isExecuting();
    mBtnPrimary->setEnabled(!busy && checkedLeafCount() > 0);
}

void TrustSafetyPreviewDialog::updateStatusBar()
{
    qint64 total = 0;
    int count = 0;
    for (const TrustSafetyActionItem &item : checkedItems()) {
        count++;
        if (item.estimatedSizeBytes >= 0)
            total += item.estimatedSizeBytes;
    }
    mLblStatusBar->setText(tr("%1 selected · %2")
        .arg(count).arg(FormatUtil::formatBytes(static_cast<quint64>(qMax<qint64>(0, total)))));
}

QList<TrustSafetyActionItem> TrustSafetyPreviewDialog::checkedItems() const
{
    QList<TrustSafetyActionItem> result;
    for (auto it = mItemRows.constBegin(); it != mItemRows.constEnd(); ++it)
        if (it.value()->checkState(0) == Qt::Checked)
            result.append(mItemsById.value(it.key()));
    return result;
}

int TrustSafetyPreviewDialog::checkedLeafCount() const
{
    int n = 0;
    for (auto it = mItemRows.constBegin(); it != mItemRows.constEnd(); ++it)
        if (it.value()->checkState(0) == Qt::Checked)
            n++;
    return n;
}
