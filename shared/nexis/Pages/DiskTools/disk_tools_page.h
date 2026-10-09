#ifndef DISK_TOOLS_PAGE_H
#define DISK_TOOLS_PAGE_H

#include <QWidget>
#include <QAtomicInt>
#include <QFuture>
#include <QTreeWidget>

#include "Services/duplicate_finder_service.h"

class QButtonGroup;
class QLabel;
class QSpinBox;
class QVBoxLayout;
class QComboBox;
class QLineEdit;
class QListWidget;
class QProgressBar;
class QPushButton;
class QResizeEvent;
class AppManager;
class SignalMapper;

namespace Ui {
    class DiskToolsPage;
}

class DiskToolsPage : public QWidget
{
    Q_OBJECT

public:
    explicit DiskToolsPage(QWidget *parent = nullptr, DuplicateFinderService *dupService = nullptr);
    ~DiskToolsPage();

signals:
    void largeOldScanFinishedS(const QList<QFileInfo> &results);
    void largeOldScanCancelledS();

private slots:
    void switchMode(int index);
    void openWipeFreeSpaceDialog();
    void addDirectory();
    void removeDirectory();
    void onLargeOldScan();
    void onLargeOldScanFinished(const QList<QFileInfo> &results);
    void onLargeOldCancelled();
    void onLargeOldTrash();
    void onDupScan();
    void onDupProgress(int stage, int current, int total, const QString &message);
    void onDupScanFinished(const QList<DuplicateGroup> &results);
    void onDupCancelled();
    void onServiceScanCancelled();
    void onLargestScan();
    void onLargestScanFinished(const QList<LargeFileEntry> &results);
    void onEmptyFoldersScan();
    void onEmptyFoldersScanFinished(const QStringList &folders);
    void onDupTrash();
    void updateLargeOldSelection();
    void updateDupSelection();

private:
    // A flat, checkable results page driven by DuplicateFinderService:
    // Largest Files and Empty Folders share this shape.
    struct FlatMode {
        QListWidget *dirList = nullptr;
        QSpinBox *spinTopN = nullptr;
        QPushButton *btnScan = nullptr;
        QPushButton *btnCancel = nullptr;
        QPushButton *btnTrash = nullptr;
        QTreeWidget *tree = nullptr;
        QWidget *emptyState = nullptr;
        QProgressBar *busy = nullptr;
        QLabel *lblStatus = nullptr;
        QLabel *lblSelection = nullptr;
        int sizeColumn = -1;
    };
    enum class ServiceScan { None, Duplicates, Largest, EmptyFolders };

    void init();
    QListWidget *buildDirPicker(QVBoxLayout *pageLayout);
    void buildFlatModePage(QWidget *page, FlatMode &mode, const QStringList &columns,
                           const QString &emptyText, const QString &scanText);
    bool beginServiceScan(ServiceScan kind, QLabel *statusLabel, const QListWidget *dirList);
    QStringList directoriesOf(const QListWidget *list) const;
    void setFlatModeScanning(FlatMode &mode, bool scanning);
    void finishFlatScan(FlatMode &mode, const QString &status);
    void trashFlatSelection(FlatMode &mode, bool folders);
    void updateFlatSelection(FlatMode &mode, bool folders);
    void buildLargeOldPage();
    void buildDuplicatePage();
    void refreshThemeColors();
    void applyLargeOldFilterLayout(bool compact);
    void resizeEvent(QResizeEvent *event) override;
    static QWidget *makeElevatedContainer(QWidget *parent);
    static QWidget *makeEmptyState(QWidget *parent, const QString &heading,
                                    const QString &text, QPushButton **outButton,
                                    const QString &buttonText);
    static void buildSectionHeader(QWidget *headerContainer, const QString &title);

private:
    Ui::DiskToolsPage *ui;
    AppManager *mAppManager;
    SignalMapper *mSignalMapper;
    DuplicateFinderService *mDupService;

    QButtonGroup *mModeGroup;

    // Directory picker (shared data, separate widgets per mode)
    QListWidget *mDirListLargeOld;
    QListWidget *mDirListDup;
    QList<QListWidget *> mDirLists;

    FlatMode mLargest;
    FlatMode mEmptyFolders;
    ServiceScan mActiveServiceScan = ServiceScan::None;

    // Large & Old mode — filter widgets
    QLabel *mLblSize = nullptr;
    QLabel *mLblNotAccessed = nullptr;
    QLabel *mLblMatch = nullptr;
    QWidget *mLargeOldFilterWidget = nullptr;
    bool mLargeOldFilterCompact = false;
    int mLargeOldFilterFullRowWidth = 0;
    QSpinBox *mSpinSize;
    QComboBox *mCbSizeUnit;
    QSpinBox *mSpinAge;
    QComboBox *mCbAgeUnit;
    QComboBox *mCbFilterMode;
    QPushButton *mBtnLargeOldScan;
    QPushButton *mBtnLargeOldCancel;
    QTreeWidget *mTreeLargeOld;
    QWidget *mEmptyStateLargeOld = nullptr;
    QLabel *mLblLargeOldStatus;
    QLabel *mLblLargeOldSelection;
    QPushButton *mBtnLargeOldTrash;

    // Duplicate mode
    QSpinBox *mSpinMinDupSize;
    QComboBox *mCbMinDupUnit;
    QLineEdit *mEditGlob;
    QPushButton *mBtnDupScan;
    QPushButton *mBtnDupCancel;
    QTreeWidget *mTreeDuplicates;
    QWidget *mEmptyStateDup = nullptr;
    QProgressBar *mDupProgress;
    QLabel *mLblDupStatus;
    QLabel *mLblDupSelection;
    QPushButton *mBtnDupTrash;

    // State
    QAtomicInt mLargeOldCancelled{0};
    QFuture<void> mLargeOldFuture;
    QList<QFileInfo> mLargeOldResults;

    // FW-08 (SSO-3736): the latest duplicate-finder result set, retained so
    // onDupTrash() can hand it back to DuplicateFinderService::trashFiles()
    // and let the service enforce the never-delete-last-copy invariant
    // plus the cleaner exclusion engine.
    QList<DuplicateGroup> mDupResults;
};

#endif // DISK_TOOLS_PAGE_H
