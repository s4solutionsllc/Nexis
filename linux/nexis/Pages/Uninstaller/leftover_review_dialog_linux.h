#ifndef LEFTOVER_REVIEW_DIALOG_LINUX_H
#define LEFTOVER_REVIEW_DIALOG_LINUX_H

#include <QDialog>
#include <QStringList>

class QCheckBox;
class QLabel;
class QPushButton;
class QTableWidget;

// SSO-15385: shown after a Linux package uninstall. Lists residual files
// found in ~/.config, ~/.cache, ~/.local/share, and autostart entries that
// match the removed packages' names.
//
// CISO §1 (SSO-15373): default action is move-to-trash (freedesktop.org
// Trash via QFile::moveToTrash). Permanent delete is NOT offered here.
// CISO §2: deny-list check is enforced in PackageToolLinux::trashLeftovers.
// CISO §3: audit log is written in PackageToolLinux::trashLeftovers.
class LeftoverReviewDialogLinux : public QDialog
{
    Q_OBJECT

public:
    explicit LeftoverReviewDialogLinux(const QStringList &packageNames, QWidget *parent = nullptr);

private slots:
    void onMoveToTrash();
    void onTableItemChanged();

private:
    void buildUI();
    void populate();
    void updateSummary();

    QStringList mPackageNames;

    QLabel        *mLblSummary  = nullptr;
    QTableWidget  *mTable       = nullptr;
    QPushButton   *mBtnTrash    = nullptr;
    QPushButton   *mBtnSkip     = nullptr;
    QCheckBox     *mChkSelectAll = nullptr;

    qint64 mTotalBytes  = 0;
    int    mItemCount   = 0;
    bool   mUpdatingAll = false;
};

#endif // LEFTOVER_REVIEW_DIALOG_LINUX_H
