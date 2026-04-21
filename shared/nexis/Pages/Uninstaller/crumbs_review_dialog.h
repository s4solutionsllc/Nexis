#ifndef CRUMBS_REVIEW_DIALOG_H
#define CRUMBS_REVIEW_DIALOG_H

#include <QDialog>
#include <QStringList>

class QTableWidget;
class QLabel;
class QPushButton;

// FR-123: shown after a macOS Uninstaller run. Lists residual files found
// in ~/Library/* matching the uninstalled apps' bundle identifiers. User
// can uncheck any rows they want to keep, then Delete Selected moves the
// remainder to the Trash via QFile::moveToTrash.
class CrumbsReviewDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CrumbsReviewDialog(const QStringList &bundleIds, QWidget *parent = nullptr);

private slots:
    void onDeleteSelected();

private:
    void buildUI();
    void populate();
    void updateSummary();

    QStringList mBundleIds;

    QLabel *mLblSummary = nullptr;
    QTableWidget *mTable = nullptr;
    QPushButton *mBtnDelete = nullptr;
    QPushButton *mBtnSkip = nullptr;
};

#endif // CRUMBS_REVIEW_DIALOG_H
