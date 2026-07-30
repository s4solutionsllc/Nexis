#ifndef CRUMBS_REVIEW_DIALOG_H
#define CRUMBS_REVIEW_DIALOG_H

#include <QDialog>
#include <QList>
#include <QStringList>

#include "Tools/crumbs_scanner.h"

class QTableWidget;
class QTableWidgetItem;
class QLabel;
class QPushButton;

// FR-123 / SSO-15384: shown after a macOS Uninstaller run. Lists residual
// files found in ~/Library/* matching the uninstalled apps' bundle identifiers.
// User reviews unchecked items (CISO §5: orphan matches default unchecked),
// selects items to delete, then "Move to Trash" removes them via
// QFile::moveToTrash — no shell/osascript surface (SSO-3366).
//
// SSO-15567: the scan runs asynchronously via CrumbsScanRunner. The table
// fills in as matches are discovered so the dialog never blocks the UI
// thread on a large ~/Library tree; interaction (checkboxes, Move to Trash)
// is held off until the scan finishes and the list has settled into its
// final sort-by-size-desc order.
class CrumbsReviewDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CrumbsReviewDialog(const QStringList &bundleIds, QWidget *parent = nullptr);

private slots:
    void onDeleteSelected();
    void onItemChanged(QTableWidgetItem *item);
    void onItemFound(CrumbsScanner::CrumbCandidate item);
    void onScanFinished(QList<CrumbsScanner::CrumbCandidate> items);

private:
    void buildUI();
    void startScan();
    void addRow(int row, const CrumbsScanner::CrumbCandidate &c);
    void populate(const QList<CrumbsScanner::CrumbCandidate> &crumbs);
    void updateDeleteButton();

    QStringList mBundleIds;
    CrumbsScanRunner *mScanRunner = nullptr;

    QLabel       *mLblSummary  = nullptr;
    QTableWidget *mTable       = nullptr;
    QPushButton  *mBtnDelete   = nullptr;
    QPushButton  *mBtnSkip     = nullptr;

    qint64 mTotalBytes = 0;
};

#endif // CRUMBS_REVIEW_DIALOG_H
