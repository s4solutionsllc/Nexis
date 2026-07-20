// SSO-15380: Trust & Safety shared component — itemized preview / confirm /
// progress dialog. See shared/nexis/Common/README.md for the adoption guide.

#ifndef TRUST_SAFETY_PREVIEW_DIALOG_H
#define TRUST_SAFETY_PREVIEW_DIALOG_H

#include "trust_safety_runner.h"
#include "trust_safety_types.h"

#include <QColor>
#include <QDialog>
#include <QFont>
#include <QMap>
#include <QSet>

class QCheckBox;
class QHeaderView;
class QLabel;
class QProgressBar;
class QPushButton;
class QTreeWidget;
class QTreeWidgetItem;
class AppManager;

class TrustSafetyPreviewDialog : public QDialog
{
    Q_OBJECT

public:
    // Per-surface configuration. Nothing here lets an adopter opt out of a
    // Design Anchor constraint (SSO-1785) — only copy/labels are configurable.
    struct Config {
        QString windowTitle;
        QString primaryActionLabel;       // e.g. "Clean Selected", "Uninstall Selected"
        // One-sentence confirmation copy shown before a REAL (non-dry-run)
        // run, e.g. "This will permanently delete the selected items."
        // Debug builds assert this is a single sentence (Design Anchor:
        // confirmation dialog copy is one sentence maximum).
        QString confirmationSentence;
    };

    explicit TrustSafetyPreviewDialog(TrustSafetyActionProvider *provider,
                                       Config config,
                                       QWidget *parent = nullptr,
                                       AppManager *appManager = nullptr);
    ~TrustSafetyPreviewDialog() override;

    // Result of the most recently completed (or cancelled) execution.
    // Default-constructed (totalItemsRequested == 0) if nothing has run yet.
    TrustSafetyRunSummary lastRunSummary() const { return mLastSummary; }

private slots:
    void onItemDiscovered(TrustSafetyActionItem item);
    void onScanFinished(QList<TrustSafetyActionItem> items);
    void onScanCancelled();
    void onExecutionProgress(int itemsDone, int itemsTotal, qint64 bytesFreedSoFar);
    void onExecutionFinished(TrustSafetyRunSummary summary);

    void onPrimaryActionClicked();
    void onCancelClicked();
    void onDryRunToggled(bool checked);
    void onSelectAllToggled(bool checked);
    void onItemCheckChanged(QTreeWidgetItem *item, int column);

    void refreshThemeColors();

private:
    void buildUI();
    void updateControlsForState();
    void updateStatusBar();
    void updatePrimaryActionEnabled();
    void applyRiskStyling();

    QTreeWidgetItem *categoryRow(const QString &categoryId, const QString &categoryLabel);
    void addOrUpdateItemRow(const TrustSafetyActionItem &item);
    void updateCategorySummary(QTreeWidgetItem *category);
    bool confirmRiskyCategory(const QString &categoryId);

    QList<TrustSafetyActionItem> checkedItems() const;
    int checkedLeafCount() const;

    TrustSafetyActionProvider *mProvider;
    Config mConfig;
    AppManager *mAppManager;
    TrustSafetyRunner *mRunner;

    QLabel *mLblTitle = nullptr;
    QLabel *mLblScanStatus = nullptr;
    QCheckBox *mChkSelectAll = nullptr;
    QCheckBox *mChkDryRun = nullptr;
    QTreeWidget *mTree = nullptr;
    QLabel *mLblStatusBar = nullptr;
    QProgressBar *mProgressBar = nullptr;
    QPushButton *mBtnPrimary = nullptr;
    QPushButton *mBtnCancel = nullptr;
    QFont mMonoFont;

    QMap<QString, QTreeWidgetItem *> mCategoryRows;   // categoryId -> row
    QMap<QString, QString> mCategoryLabels;           // categoryId -> raw label (no count suffix)
    QMap<QString, QTreeWidgetItem *> mItemRows;       // itemId -> row
    QMap<QString, TrustSafetyActionItem> mItemsById;
    QSet<QString> mRiskyConfirmedCategoryIds;         // categories the user already confirmed this session

    TrustSafetyRunSummary mLastSummary;
    QColor mWarningColor;
    bool mSuppressCheckSignal = false;
};

#endif // TRUST_SAFETY_PREVIEW_DIALOG_H
