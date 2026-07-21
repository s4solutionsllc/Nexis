#ifndef CRUMBS_REVIEW_DIALOG_H
#define CRUMBS_REVIEW_DIALOG_H

#include <QDialog>
#include <QStringList>

class QTableWidget;
class QTableWidgetItem;
class QLabel;
class QPushButton;

// FR-123 / SSO-15384: shown after a macOS Uninstaller run. Lists residual
// files found in ~/Library/* matching the uninstalled apps' bundle identifiers.
// User reviews unchecked items (CISO §5: orphan matches default unchecked),
// selects items to delete, then "Move to Trash" removes them via
// QFile::moveToTrash — no shell/osascript surface (SSO-3366).
class CrumbsReviewDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CrumbsReviewDialog(const QStringList &bundleIds, QWidget *parent = nullptr);

private slots:
    void onDeleteSelected();
    void onItemChanged(QTableWidgetItem *item);

private:
    void buildUI();
    void populate();
    void updateDeleteButton();

    QStringList mBundleIds;

    QLabel       *mLblSummary  = nullptr;
    QTableWidget *mTable       = nullptr;
    QPushButton  *mBtnDelete   = nullptr;
    QPushButton  *mBtnSkip     = nullptr;

    qint64 mTotalBytes = 0;
};

#endif // CRUMBS_REVIEW_DIALOG_H
