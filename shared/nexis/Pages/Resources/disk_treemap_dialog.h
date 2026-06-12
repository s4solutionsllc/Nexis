// SSO-3737 / FW-09: built-in disk-space visualizer dialog.
//
// Wraps DirSizeScanner + TreemapView with a small toolbar (folder picker,
// scan/cancel, drill-up) and a status bar. Reveal / move-to-trash actions
// are routed through FileSearchService so they share the cleaner's trash
// path on Linux and macOS.

#ifndef DISK_TREEMAP_DIALOG_H
#define DISK_TREEMAP_DIALOG_H

#include <QDialog>

class QLabel;
class QPushButton;
class QProgressBar;
class QComboBox;

#include "Managers/dir_size_scanner.h"

class AppManager;
class SignalMapper;
class TreemapView;

class DiskTreemapDialog : public QDialog
{
    Q_OBJECT

public:
    explicit DiskTreemapDialog(QWidget *parent = nullptr,
                               AppManager *appManager = nullptr,
                               SignalMapper *signalMapper = nullptr);

    void prefillVolumes(const QStringList &volumeRoots);

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void onChooseFolder();
    void onScanClicked();
    void onCancelClicked();
    void onDrillUpClicked();
    void onScanFinished(DirSizeNodePtr root);
    void onScanCancelled();
    void onScanProgress(qint64 bytes, int files);
    void onTileHovered(DirSizeNode *node);
    void onDrillRequested(DirSizeNode *node);
    void onRevealRequested(DirSizeNode *node);
    void onTrashRequested(DirSizeNode *node);
    void applyThemeColors();

private:
    void setBusy(bool busy);
    void startScan(const QString &path);
    void updateBreadcrumb();

    AppManager   *mAppManager   = nullptr;
    SignalMapper *mSignalMapper = nullptr;

    DirSizeScanner *mScanner = nullptr;

    QComboBox    *mFolderCombo   = nullptr;
    QPushButton  *mChooseButton  = nullptr;
    QPushButton  *mScanButton    = nullptr;
    QPushButton  *mCancelButton  = nullptr;
    QPushButton  *mDrillUpButton = nullptr;
    QLabel       *mBreadcrumb    = nullptr;
    QLabel       *mStatusLabel   = nullptr;
    QProgressBar *mProgress      = nullptr;
    TreemapView  *mView          = nullptr;
};

#endif // DISK_TREEMAP_DIALOG_H
