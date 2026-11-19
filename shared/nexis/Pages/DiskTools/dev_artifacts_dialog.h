#ifndef DEV_ARTIFACTS_DIALOG_H
#define DEV_ARTIFACTS_DIALOG_H

#include <QAtomicInt>
#include <QDialog>
#include <QFutureWatcher>

#include "Tools/dev_artifact_scanner.h"

class QCloseEvent;
class QLabel;
class QListWidget;
class QProgressBar;
class QPushButton;
class QSpinBox;
class QTreeWidget;

// SSO-27098: "Developer Projects" cleaner. The user picks the folders they keep
// code in; Nexis lists each stale project once, with the size of its
// rebuildable folders (node_modules, .venv, target, build, Pods, .gradle) and
// how long the project itself has been idle, and moves the ticked ones to the
// Trash. Every safety decision lives in DevArtifacts (core); this dialog only
// gathers input and reports. It is opened by hand and never reachable from the
// scheduler or the headless --clean path.
class DevArtifactsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit DevArtifactsDialog(QWidget *parent = nullptr);
    ~DevArtifactsDialog() override;

private slots:
    void onAddRoot();
    void onRemoveRoot();
    void onScan();
    void onScanFinished();
    void onTrash();
    void onTrashFinished();
    void populateResults();
    void updateSelection();

private:
    void buildUi();
    void setBusy(bool busy, const QString &status = QString());
    QStringList roots() const;
    void saveRoots() const;
    QList<DevArtifacts::TrashRequest> checkedRequests() const;
    void selectionTotals(int &projects, quint64 &bytes) const;
    void closeEvent(QCloseEvent *event) override;
    void reject() override;

    QListWidget *mRootList = nullptr;
    QPushButton *mBtnAddRoot = nullptr;
    QPushButton *mBtnRemoveRoot = nullptr;
    QSpinBox *mSpinIdleDays = nullptr;
    QPushButton *mBtnScan = nullptr;
    QPushButton *mBtnCancelScan = nullptr;
    QProgressBar *mBusy = nullptr;
    QTreeWidget *mTree = nullptr;
    QLabel *mLblStatus = nullptr;
    QLabel *mLblSelection = nullptr;
    QPushButton *mBtnTrash = nullptr;

    DevArtifacts::ScanResult mResult;
    QAtomicInt mCancelScan{0};
    QFutureWatcher<DevArtifacts::ScanResult> mScanWatcher;
    QFutureWatcher<QList<DevArtifacts::TrashOutcome>> mTrashWatcher;
};

#endif // DEV_ARTIFACTS_DIALOG_H
