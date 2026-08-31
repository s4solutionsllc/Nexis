// SSO-3737 / FW-09: built-in disk-space visualizer dialog.
// SSO-23862: added the bubble-map/sunburst visualization modes + picker.
//
// Wraps DirSizeScanner + a DiskMapView (treemap / bubble-map / sunburst,
// switchable live) with a small toolbar (folder picker, scan/cancel,
// drill-up, visualization picker) and a status bar. Reveal / move-to-trash
// actions are routed through FileSearchService so they share the cleaner's
// trash path on Linux and macOS.
//
// All three views are kept in lockstep: every drill/scan/theme change is
// applied to all of them, not just the currently visible one, so switching
// visualization types is instant and never re-scans or re-derives from a
// different snapshot of the tree.

#ifndef DISK_TREEMAP_DIALOG_H
#define DISK_TREEMAP_DIALOG_H

#include <QDialog>
#include <QVector>

class QLabel;
class QPushButton;
class QProgressBar;
class QComboBox;
class QStackedWidget;

#include "Managers/dir_size_scanner.h"
#include "Services/file_search_service.h"

class AppManager;
class SignalMapper;
class DiskMapView;

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
    void onVisualizationChanged(int index);
    void onFileOperationFinished(FileSearchService::FileOperation op,
                                 QString filePath, bool hadError,
                                 QString errorMessage);
    void applyThemeColors();

private:
    void setBusy(bool busy);
    void startScan(const QString &path);
    void updateBreadcrumb();

    AppManager   *mAppManager   = nullptr;
    SignalMapper *mSignalMapper = nullptr;

    DirSizeScanner *mScanner = nullptr;

    QComboBox    *mFolderCombo    = nullptr;
    QPushButton  *mChooseButton   = nullptr;
    QPushButton  *mScanButton     = nullptr;
    QPushButton  *mCancelButton   = nullptr;
    QPushButton  *mDrillUpButton  = nullptr;
    QComboBox    *mVisPicker      = nullptr;
    QLabel       *mBreadcrumb     = nullptr;
    QLabel       *mStatusLabel    = nullptr;
    QProgressBar *mProgress       = nullptr;

    // All three visualization modes, kept in lockstep and pre-built so
    // switching between them (mStack) never re-scans or re-lays-out on
    // demand. mView is whichever one is currently visible.
    QStackedWidget      *mStack = nullptr;
    QVector<DiskMapView*> mViews;
    DiskMapView          *mView = nullptr;

    // Root path of the current scan, kept independent of mFolderCombo's
    // (editable) text so a post-trash refresh always re-scans what's
    // actually on screen, not whatever the user has since typed.
    QString mLastScannedPath;
};

#endif // DISK_TREEMAP_DIALOG_H
