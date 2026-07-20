#ifndef WIPE_FREE_SPACE_DIALOG_H
#define WIPE_FREE_SPACE_DIALOG_H

#include <QDialog>
#include <QList>

#include "Services/wipe_free_space_service.h"

class QCloseEvent;
class QLabel;
class QListWidget;
class QProgressBar;
class QPushButton;
class QStackedWidget;

// SSO-15382: "Wipe Free Space" flow — pick a mounted, writable volume and
// preview what will happen (step 0), then run the fill-and-delete pass or
// the TRIM-based reclaim with live progress (step 1).
class WipeFreeSpaceDialog : public QDialog
{
    Q_OBJECT

public:
    explicit WipeFreeSpaceDialog(QWidget *parent = nullptr);
    ~WipeFreeSpaceDialog() override;

private slots:
    void onVolumeSelectionChanged();
    void onWipeClicked();
    void onStopClicked();
    void onProgress(qint64 bytesWritten, qint64 estimatedTotalBytes, const QString &message);
    void onFinished(bool success, const QString &message);
    void onCancelled();

private:
    void buildUi();
    void populateVolumes();
    void updatePreview();
    void closeEvent(QCloseEvent *event) override;

    QStackedWidget *mStack = nullptr;

    // Step 0 — pick + preview
    QListWidget *mVolumeList = nullptr;
    QLabel *mLblPreview = nullptr;
    QPushButton *mBtnWipe = nullptr;

    // Step 1 — progress
    QLabel *mLblProgress = nullptr;
    QProgressBar *mProgressBar = nullptr;
    QPushButton *mBtnStop = nullptr;
    QPushButton *mBtnClose = nullptr;

    QList<WipeTarget> mTargets;
    WipeFreeSpaceService *mService;
    bool mRunning = false;
};

#endif // WIPE_FREE_SPACE_DIALOG_H
