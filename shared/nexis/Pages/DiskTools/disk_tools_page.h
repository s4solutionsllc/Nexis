#ifndef DISK_TOOLS_PAGE_H
#define DISK_TOOLS_PAGE_H

#include <QWidget>
#include <QFuture>
#include <QTreeWidget>

#include "Services/duplicate_finder_service.h"

class QButtonGroup;
class QLabel;
class QSpinBox;
class QComboBox;
class QLineEdit;
class QListWidget;
class QProgressBar;
class QPushButton;
class AppManager;
class SignalMapper;
class FileSearchService;

namespace Ui {
    class DiskToolsPage;
}

class DiskToolsPage : public QWidget
{
    Q_OBJECT

public:
    explicit DiskToolsPage(QWidget *parent = nullptr);
    ~DiskToolsPage();

signals:
    void largeOldScanFinishedS();

private slots:
    void switchMode(int index);
    void addDirectory();
    void removeDirectory();
    void onLargeOldScan();
    void onLargeOldScanFinished();
    void onLargeOldTrash();
    void onDupScan();
    void onDupProgress(int stage, int current, int total, const QString &message);
    void onDupScanFinished(const QList<DuplicateGroup> &results);
    void onDupCancelled();
    void onDupTrash();
    void updateLargeOldSelection();
    void updateDupSelection();

private:
    void init();
    void buildLargeOldPage();
    void buildDuplicatePage();
    void refreshThemeColors();
    void populateDefaultDirectories();

private:
    Ui::DiskToolsPage *ui;
    AppManager *mAppManager;
    SignalMapper *mSignalMapper;
    DuplicateFinderService *mDupService;

    QButtonGroup *mModeGroup;

    // Directory picker (shared data, separate widgets per mode)
    QListWidget *mDirListLargeOld;
    QListWidget *mDirListDup;

    // Large & Old mode
    QSpinBox *mSpinSize;
    QComboBox *mCbSizeUnit;
    QSpinBox *mSpinAge;
    QComboBox *mCbAgeUnit;
    QComboBox *mCbFilterMode;
    QPushButton *mBtnLargeOldScan;
    QTreeWidget *mTreeLargeOld;
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
    QProgressBar *mDupProgress;
    QLabel *mLblDupStatus;
    QLabel *mLblDupSelection;
    QPushButton *mBtnDupTrash;

    // State
    bool mLargeOldScanInProgress = false;
    QFuture<void> mLargeOldFuture;
    QList<QFileInfo> mLargeOldResults;
};

#endif // DISK_TOOLS_PAGE_H
