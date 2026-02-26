# FR-62 & FR-63: Disk Tools Page Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Add a "Disk Tools" sidebar page with two modes — Large & Old File Scanner (FR-62) and Duplicate File Finder (FR-63).

**Architecture:** Single `DiskToolsPage` with a `QStackedWidget` toggled by segmented buttons. FR-62 reuses `FileSearchService` for `find`-based scanning. FR-63 uses a new `DuplicateFinderService` with 3-stage size→partial-hash→full-hash pipeline. Both share a directory picker and "Move to Trash" action bar.

**Tech Stack:** C++17, Qt6 (Widgets, Concurrent), QCryptographicHash (SHA-256), QDirIterator

---

## Task 1: Create Sidebar Icons

**Files:**
- Create: `shared/nexis/static/themes/default/img/sidebar-icons/disk-tools.svg`
- Create: `shared/nexis/static/themes/light/img/sidebar-icons/disk-tools.svg`
- Modify: `shared/nexis/static.qrc`

**Step 1: Create the dark-theme icon SVG**

Create `shared/nexis/static/themes/default/img/sidebar-icons/disk-tools.svg`:

```xml
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 16 16">
  <path fill="#ffffff" d="M8 1a7 7 0 100 14A7 7 0 008 1zm0 1.5a5.5 5.5 0 110 11 5.5 5.5 0 010-11zM8 5a.75.75 0 00-.75.75v2.5H5a.75.75 0 000 1.5h2.25v2.5a.75.75 0 001.5 0v-2.5H11a.75.75 0 000-1.5H8.75v-2.5A.75.75 0 008 5z"/>
  <path fill="#ffffff" d="M3.5 2.5L1.5 4v1l2-1.5L5.5 5V4z" opacity=".7"/>
  <path fill="#ffffff" d="M12.5 2.5L14.5 4v1l-2-1.5L10.5 5V4z" opacity=".7"/>
</svg>
```

Wait — that's a generic icon. We need a disk + magnifying glass. Let me use a simpler approach: a hard drive with a magnifying glass overlay, matching the existing icon style (16x16 viewBox, single-path, fill color `#ffffff` for dark / `#3d3846` for light).

```xml
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 16 16">
  <path fill="#ffffff" d="M2 3.5A1.5 1.5 0 013.5 2h9A1.5 1.5 0 0114 3.5v4a1.5 1.5 0 01-1.5 1.5H10a4.5 4.5 0 00-1-1h3.5a.5.5 0 00.5-.5v-4a.5.5 0 00-.5-.5h-9a.5.5 0 00-.5.5v4a.5.5 0 00.5.5H5a4.5 4.5 0 00-.26 1H3.5A1.5 1.5 0 012 7.5v-4zM11 5a.5.5 0 11-1 0 .5.5 0 011 0z"/>
  <path fill="#ffffff" d="M5.5 8a2.5 2.5 0 100 5 2.5 2.5 0 000-5zM2 10.5a3.5 3.5 0 116.45 1.89l2.08 2.08a.5.5 0 01-.7.7l-2.08-2.08A3.5 3.5 0 012 10.5z"/>
</svg>
```

**Step 2: Create the light-theme icon SVG**

Create `shared/nexis/static/themes/light/img/sidebar-icons/disk-tools.svg` — same SVG but with `fill="#3d3846"` instead of `fill="#ffffff"`.

**Step 3: Register icons in static.qrc**

Add these two lines to `shared/nexis/static.qrc` after line 110 (after the `docker.svg` entry in the default section) and after line 129 (after `docker.svg` in the light section):

In the default theme block (after line 110):
```xml
        <file>static/themes/default/img/sidebar-icons/disk-tools.svg</file>
```

In the light theme block (after line 129):
```xml
        <file>static/themes/light/img/sidebar-icons/disk-tools.svg</file>
```

**Step 4: Build to verify QRC compiles**

Run: `cmake --build build -j$(sysctl -n hw.ncpu) 2>&1 | tail -5`
Expected: Build succeeds (icons bundled but not yet referenced)

**Step 5: Commit**

```bash
git add shared/nexis/static/themes/default/img/sidebar-icons/disk-tools.svg \
        shared/nexis/static/themes/light/img/sidebar-icons/disk-tools.svg \
        shared/nexis/static.qrc
git commit -m "feat(disk-tools): add sidebar icons for Disk Tools page (FR-62, FR-63)"
```

---

## Task 2: Create DuplicateFinderService

**Files:**
- Create: `shared/nexis/Services/duplicate_finder_service.h`
- Create: `shared/nexis/Services/duplicate_finder_service.cpp`
- Modify: `CMakeLists.txt` (add to GUI_SHARED_SRCS and GUI_SHARED_HDRS)

**Step 1: Create the service header**

Create `shared/nexis/Services/duplicate_finder_service.h`:

```cpp
#ifndef DUPLICATE_FINDER_SERVICE_H
#define DUPLICATE_FINDER_SERVICE_H

#include <QObject>
#include <QFileInfo>
#include <QAtomicInt>
#include <QFuture>

struct DuplicateGroup {
    QList<QFileInfo> files;
    quint64 fileSize = 0;
    QByteArray hash;
};

class DuplicateFinderService : public QObject
{
    Q_OBJECT

public:
    static DuplicateFinderService *ins();

    void scan(const QStringList &directories, qint64 minSize,
              const QString &globFilter = QString());
    void cancel();
    bool isScanning() const;

signals:
    void progressUpdated(int stage, int current, int total, const QString &message);
    void scanFinished(const QList<DuplicateGroup> &results);

private:
    explicit DuplicateFinderService(QObject *parent = nullptr);
    static DuplicateFinderService *instance;

    QList<DuplicateGroup> runPipeline(const QStringList &directories,
                                       qint64 minSize,
                                       const QString &globFilter);

    QAtomicInt mCancelled{0};
    QFuture<void> mWorkerFuture;
};

#endif // DUPLICATE_FINDER_SERVICE_H
```

**Step 2: Create the service implementation**

Create `shared/nexis/Services/duplicate_finder_service.cpp`:

```cpp
#include "duplicate_finder_service.h"

#include <QCryptographicHash>
#include <QDirIterator>
#include <QRegularExpression>
#include <QtConcurrent>

DuplicateFinderService *DuplicateFinderService::instance = nullptr;

DuplicateFinderService *DuplicateFinderService::ins()
{
    if (!instance)
        instance = new DuplicateFinderService;
    return instance;
}

DuplicateFinderService::DuplicateFinderService(QObject *parent)
    : QObject(parent)
{
    qRegisterMetaType<QList<DuplicateGroup>>("QList<DuplicateGroup>");
}

bool DuplicateFinderService::isScanning() const
{
    return mWorkerFuture.isRunning();
}

void DuplicateFinderService::cancel()
{
    mCancelled.storeRelaxed(1);
}

void DuplicateFinderService::scan(const QStringList &directories,
                                   qint64 minSize,
                                   const QString &globFilter)
{
    if (mWorkerFuture.isRunning())
        return;

    mCancelled.storeRelaxed(0);

    mWorkerFuture = QtConcurrent::run([this, directories, minSize, globFilter]() {
        QList<DuplicateGroup> results = runPipeline(directories, minSize, globFilter);
        emit scanFinished(results);
    });
}

static QByteArray hashFilePartial(const QString &path, qint64 bytes = 4096)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return {};
    QByteArray data = f.read(bytes);
    f.close();
    return QCryptographicHash::hash(data, QCryptographicHash::Sha256);
}

static QByteArray hashFileFull(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return {};
    QCryptographicHash hasher(QCryptographicHash::Sha256);
    const qint64 chunkSize = 65536;
    while (!f.atEnd()) {
        hasher.addData(f.read(chunkSize));
    }
    f.close();
    return hasher.result();
}

QList<DuplicateGroup> DuplicateFinderService::runPipeline(
    const QStringList &directories, qint64 minSize, const QString &globFilter)
{
    // Convert glob to regex if provided
    QRegularExpression globRe;
    if (!globFilter.isEmpty()) {
        QString pattern = QRegularExpression::wildcardToRegularExpression(globFilter);
        globRe.setPattern(pattern);
        globRe.setPatternOptions(QRegularExpression::CaseInsensitiveOption);
    }

    // --- Stage 1: Collect files and group by size ---
    QHash<qint64, QList<QFileInfo>> sizeGroups;
    int totalScanned = 0;

    for (const QString &dir : directories) {
        if (mCancelled.loadRelaxed())
            return {};

        QDirIterator it(dir, QDir::Files | QDir::NoDotAndDotDot,
                        QDirIterator::Subdirectories);
        while (it.hasNext()) {
            if (mCancelled.loadRelaxed())
                return {};

            it.next();
            QFileInfo info = it.fileInfo();

            if (info.size() < minSize)
                continue;

            if (globRe.isValid() && !globFilter.isEmpty()) {
                if (!globRe.match(info.fileName()).hasMatch())
                    continue;
            }

            sizeGroups[info.size()].append(info);
            totalScanned++;

            if (totalScanned % 1000 == 0)
                emit progressUpdated(1, totalScanned, 0,
                    tr("Stage 1: Scanning files... %1 found").arg(totalScanned));
        }
    }

    // Remove size groups with only one file
    QList<qint64> sizesToRemove;
    for (auto it = sizeGroups.constBegin(); it != sizeGroups.constEnd(); ++it) {
        if (it.value().size() <= 1)
            sizesToRemove.append(it.key());
    }
    for (qint64 s : sizesToRemove)
        sizeGroups.remove(s);

    // Count candidates for stage 2
    int totalCandidates = 0;
    for (auto it = sizeGroups.constBegin(); it != sizeGroups.constEnd(); ++it)
        totalCandidates += it.value().size();

    emit progressUpdated(1, totalScanned, totalScanned,
        tr("Stage 1 complete: %1 candidates in %2 size groups")
            .arg(totalCandidates).arg(sizeGroups.size()));

    if (mCancelled.loadRelaxed())
        return {};

    // --- Stage 2: Partial hash (first 4 KB) ---
    QHash<QByteArray, QList<QFileInfo>> partialHashGroups;
    int hashProgress = 0;

    for (auto it = sizeGroups.constBegin(); it != sizeGroups.constEnd(); ++it) {
        for (const QFileInfo &fi : it.value()) {
            if (mCancelled.loadRelaxed())
                return {};

            QByteArray partialHash = hashFilePartial(fi.absoluteFilePath());
            if (!partialHash.isEmpty()) {
                // Combine size + partial hash to avoid collisions across sizes
                QByteArray key = QByteArray::number(fi.size()) + partialHash;
                partialHashGroups[key].append(fi);
            }

            hashProgress++;
            if (hashProgress % 100 == 0)
                emit progressUpdated(2, hashProgress, totalCandidates,
                    tr("Stage 2: Partial hashing... %1/%2")
                        .arg(hashProgress).arg(totalCandidates));
        }
    }

    // Remove partial hash groups with only one file
    QList<QByteArray> hashesToRemove;
    for (auto it = partialHashGroups.constBegin(); it != partialHashGroups.constEnd(); ++it) {
        if (it.value().size() <= 1)
            hashesToRemove.append(it.key());
    }
    for (const QByteArray &h : hashesToRemove)
        partialHashGroups.remove(h);

    int stage3Candidates = 0;
    for (auto it = partialHashGroups.constBegin(); it != partialHashGroups.constEnd(); ++it)
        stage3Candidates += it.value().size();

    emit progressUpdated(2, totalCandidates, totalCandidates,
        tr("Stage 2 complete: %1 candidates remaining").arg(stage3Candidates));

    if (mCancelled.loadRelaxed())
        return {};

    // --- Stage 3: Full hash ---
    QHash<QByteArray, QList<QFileInfo>> fullHashGroups;
    int fullProgress = 0;

    for (auto it = partialHashGroups.constBegin(); it != partialHashGroups.constEnd(); ++it) {
        for (const QFileInfo &fi : it.value()) {
            if (mCancelled.loadRelaxed())
                return {};

            QByteArray fullHash = hashFileFull(fi.absoluteFilePath());
            if (!fullHash.isEmpty())
                fullHashGroups[fullHash].append(fi);

            fullProgress++;
            if (fullProgress % 10 == 0)
                emit progressUpdated(3, fullProgress, stage3Candidates,
                    tr("Stage 3: Full hashing... %1/%2")
                        .arg(fullProgress).arg(stage3Candidates));
        }
    }

    // Build result groups (only groups with 2+ files are duplicates)
    QList<DuplicateGroup> results;
    for (auto it = fullHashGroups.constBegin(); it != fullHashGroups.constEnd(); ++it) {
        if (it.value().size() >= 2) {
            DuplicateGroup group;
            group.files = it.value();
            group.fileSize = group.files.first().size();
            group.hash = it.key();
            results.append(group);
        }
    }

    // Sort by wasted space descending (fileSize * (count - 1))
    std::sort(results.begin(), results.end(), [](const DuplicateGroup &a, const DuplicateGroup &b) {
        return a.fileSize * (a.files.size() - 1) > b.fileSize * (b.files.size() - 1);
    });

    emit progressUpdated(3, stage3Candidates, stage3Candidates,
        tr("Scan complete: %1 duplicate groups found").arg(results.size()));

    return results;
}
```

**Step 3: Register in CMakeLists.txt**

Add to `GUI_SHARED_SRCS` (after `file_search_service.cpp`, around line 225):
```cmake
  "${GUI_SHARED_DIR}/Services/duplicate_finder_service.cpp"
```

Add to `GUI_SHARED_HDRS` (after `file_search_service.h`, around line 290):
```cmake
  "${GUI_SHARED_DIR}/Services/duplicate_finder_service.h"
```

**Step 4: Build to verify compilation**

Run: `cmake --build build -j$(sysctl -n hw.ncpu) 2>&1 | tail -5`
Expected: Build succeeds, new service compiles cleanly

**Step 5: Commit**

```bash
git add shared/nexis/Services/duplicate_finder_service.h \
        shared/nexis/Services/duplicate_finder_service.cpp \
        CMakeLists.txt
git commit -m "feat(disk-tools): add DuplicateFinderService with 3-stage hash pipeline (FR-63)"
```

---

## Task 3: Create DiskToolsPage UI and Skeleton

**Files:**
- Create: `shared/nexis/Pages/DiskTools/disk_tools_page.h`
- Create: `shared/nexis/Pages/DiskTools/disk_tools_page.cpp`
- Create: `shared/nexis/Pages/DiskTools/disk_tools_page.ui`
- Modify: `CMakeLists.txt` (sources, headers, AUTOUIC path, include dir)

**Step 1: Create the .ui file**

Create `shared/nexis/Pages/DiskTools/disk_tools_page.ui`:

```xml
<?xml version="1.0" encoding="UTF-8"?>
<ui version="4.0">
 <class>DiskToolsPage</class>
 <widget class="QWidget" name="DiskToolsPage">
  <property name="geometry">
   <rect>
    <x>0</x>
    <y>0</y>
    <width>1025</width>
    <height>736</height>
   </rect>
  </property>
  <property name="windowTitle">
   <string>Disk Tools</string>
  </property>
  <layout class="QVBoxLayout" name="mainLayout">
   <property name="spacing">
    <number>8</number>
   </property>
   <property name="leftMargin">
    <number>15</number>
   </property>
   <property name="topMargin">
    <number>10</number>
   </property>
   <property name="rightMargin">
    <number>15</number>
   </property>
   <property name="bottomMargin">
    <number>15</number>
   </property>
   <item>
    <layout class="QHBoxLayout" name="modeBarLayout">
     <property name="spacing">
      <number>0</number>
     </property>
     <item>
      <widget class="QPushButton" name="btnModeLargeOld">
       <property name="text">
        <string>Large &amp; Old Files</string>
       </property>
       <property name="checkable">
        <bool>true</bool>
       </property>
       <property name="checked">
        <bool>true</bool>
       </property>
      </widget>
     </item>
     <item>
      <widget class="QPushButton" name="btnModeDuplicates">
       <property name="text">
        <string>Duplicate Finder</string>
       </property>
       <property name="checkable">
        <bool>true</bool>
       </property>
      </widget>
     </item>
     <item>
      <spacer name="modeBarSpacer">
       <property name="orientation">
        <enum>Qt::Horizontal</enum>
       </property>
      </spacer>
     </item>
    </layout>
   </item>
   <item>
    <widget class="QStackedWidget" name="stackedModes">
     <property name="currentIndex">
      <number>0</number>
     </property>
     <widget class="QWidget" name="pageLargeOld"/>
     <widget class="QWidget" name="pageDuplicates"/>
    </widget>
   </item>
  </layout>
 </widget>
</ui>
```

**Step 2: Create the page header**

Create `shared/nexis/Pages/DiskTools/disk_tools_page.h`:

```cpp
#ifndef DISK_TOOLS_PAGE_H
#define DISK_TOOLS_PAGE_H

#include <QWidget>
#include <QFuture>
#include <QTreeWidget>

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
class DuplicateFinderService;
struct DuplicateGroup;

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

    // Directory picker
    void addDirectory();
    void removeDirectory();

    // Large & Old Files
    void onLargeOldScan();
    void onLargeOldScanFinished();
    void onLargeOldTrash();

    // Duplicate Finder
    void onDupScan();
    void onDupProgress(int stage, int current, int total, const QString &message);
    void onDupScanFinished(const QList<DuplicateGroup> &results);
    void onDupTrash();

    void updateSelectionLabel();

private:
    void init();
    void buildLargeOldPage();
    void buildDuplicatePage();
    QWidget *buildDirectoryPicker();
    QWidget *buildActionBar();
    void refreshThemeColors();
    void populateDefaultDirectories();

    QTreeWidget *currentTree() const;
    quint64 selectedSize() const;
    int selectedCount() const;

private:
    Ui::DiskToolsPage *ui;
    AppManager *mAppManager;
    SignalMapper *mSignalMapper;
    FileSearchService *mFileSearchService;
    DuplicateFinderService *mDupService;

    QButtonGroup *mModeGroup;

    // Shared widgets
    QListWidget *mDirList;
    QPushButton *mBtnAddDir;
    QPushButton *mBtnRemoveDir;
    QLabel *mLblSelection;
    QPushButton *mBtnTrash;

    // Large & Old mode widgets
    QSpinBox *mSpinSize;
    QComboBox *mCbSizeUnit;
    QSpinBox *mSpinAge;
    QComboBox *mCbAgeUnit;
    QComboBox *mCbFilterMode;
    QPushButton *mBtnLargeOldScan;
    QTreeWidget *mTreeLargeOld;
    QLabel *mLblLargeOldStatus;

    // Duplicate mode widgets
    QSpinBox *mSpinMinDupSize;
    QComboBox *mCbMinDupUnit;
    QLineEdit *mEditGlob;
    QPushButton *mBtnDupScan;
    QPushButton *mBtnDupCancel;
    QTreeWidget *mTreeDuplicates;
    QProgressBar *mDupProgress;
    QLabel *mLblDupStatus;

    // State
    bool mLargeOldScanInProgress = false;
    QFuture<void> mLargeOldFuture;

    // Large/Old scan results (written by worker, read by main thread)
    QList<QFileInfo> mLargeOldResults;
};

#endif // DISK_TOOLS_PAGE_H
```

**Step 3: Create the page implementation**

Create `shared/nexis/Pages/DiskTools/disk_tools_page.cpp`. This is the largest file — it builds the UI programmatically for both modes, handles scanning, and manages deletion.

```cpp
#include "disk_tools_page.h"
#include "ui_disk_tools_page.h"

#include <QButtonGroup>
#include <QComboBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QSpinBox>
#include <QStandardPaths>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QtConcurrent>

#include "Managers/app_manager.h"
#include "Services/file_search_service.h"
#include "Services/duplicate_finder_service.h"
#include "signal_mapper.h"
#include "utilities.h"
#include <Utils/format_util.h>
#include <Tools/file_search_tool.h>

DiskToolsPage::DiskToolsPage(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::DiskToolsPage)
    , mAppManager(AppManager::ins())
    , mSignalMapper(SignalMapper::ins())
    , mFileSearchService(FileSearchService::ins())
    , mDupService(DuplicateFinderService::ins())
{
    ui->setupUi(this);
    init();
}

DiskToolsPage::~DiskToolsPage()
{
    mLargeOldFuture.waitForFinished();
    delete ui;
}

void DiskToolsPage::init()
{
    // Mode toggle button group
    mModeGroup = new QButtonGroup(this);
    mModeGroup->setExclusive(true);
    mModeGroup->addButton(ui->btnModeLargeOld, 0);
    mModeGroup->addButton(ui->btnModeDuplicates, 1);
    connect(mModeGroup, &QButtonGroup::idClicked, this, &DiskToolsPage::switchMode);

    // Style mode buttons as segmented control
    ui->btnModeLargeOld->setCursor(Qt::PointingHandCursor);
    ui->btnModeDuplicates->setCursor(Qt::PointingHandCursor);
    ui->btnModeLargeOld->setObjectName("segmentedLeft");
    ui->btnModeDuplicates->setObjectName("segmentedRight");

    buildLargeOldPage();
    buildDuplicatePage();

    // Connect duplicate service signals
    connect(mDupService, &DuplicateFinderService::progressUpdated,
            this, &DiskToolsPage::onDupProgress);
    connect(mDupService, &DuplicateFinderService::scanFinished,
            this, &DiskToolsPage::onDupScanFinished);

    // Internal signal for large/old scan completion (worker → main thread)
    connect(this, &DiskToolsPage::largeOldScanFinishedS,
            this, &DiskToolsPage::onLargeOldScanFinished);

    // Theme changes
    connect(mSignalMapper, &SignalMapper::sigChangedAppTheme,
            this, &DiskToolsPage::refreshThemeColors);

    refreshThemeColors();
}

void DiskToolsPage::switchMode(int index)
{
    ui->stackedModes->setCurrentIndex(index);
    updateSelectionLabel();
}

// ---- Directory Picker (shared between modes) ----

QWidget *DiskToolsPage::buildDirectoryPicker()
{
    auto *frame = new QFrame(this);
    frame->setObjectName("dirPickerFrame");
    auto *layout = new QHBoxLayout(frame);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);

    mDirList = new QListWidget(frame);
    mDirList->setObjectName("dirList");
    mDirList->setMaximumHeight(80);
    populateDefaultDirectories();
    layout->addWidget(mDirList, 1);

    auto *btnLayout = new QVBoxLayout();
    btnLayout->setSpacing(4);

    mBtnAddDir = new QPushButton(tr("Add..."), frame);
    mBtnAddDir->setObjectName("btnAddDir");
    mBtnAddDir->setCursor(Qt::PointingHandCursor);
    connect(mBtnAddDir, &QPushButton::clicked, this, &DiskToolsPage::addDirectory);
    btnLayout->addWidget(mBtnAddDir);

    mBtnRemoveDir = new QPushButton(tr("Remove"), frame);
    mBtnRemoveDir->setObjectName("btnRemoveDir");
    mBtnRemoveDir->setCursor(Qt::PointingHandCursor);
    connect(mBtnRemoveDir, &QPushButton::clicked, this, &DiskToolsPage::removeDirectory);
    btnLayout->addWidget(mBtnRemoveDir);

    btnLayout->addStretch();
    layout->addLayout(btnLayout);

    return frame;
}

void DiskToolsPage::populateDefaultDirectories()
{
    mDirList->clear();
    mDirList->addItem(QStandardPaths::writableLocation(QStandardPaths::HomeLocation));
    QString downloads = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    if (!downloads.isEmpty() && downloads != QStandardPaths::writableLocation(QStandardPaths::HomeLocation))
        mDirList->addItem(downloads);
    QString documents = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    if (!documents.isEmpty() && documents != QStandardPaths::writableLocation(QStandardPaths::HomeLocation))
        mDirList->addItem(documents);
}

void DiskToolsPage::addDirectory()
{
    QString dir = QFileDialog::getExistingDirectory(this, tr("Select Directory"),
        QStandardPaths::writableLocation(QStandardPaths::HomeLocation));
    if (!dir.isEmpty()) {
        // Avoid duplicates
        for (int i = 0; i < mDirList->count(); ++i) {
            if (mDirList->item(i)->text() == dir)
                return;
        }
        mDirList->addItem(dir);
    }
}

void DiskToolsPage::removeDirectory()
{
    auto *item = mDirList->currentItem();
    if (item)
        delete item;
}

// ---- Action Bar (shared) ----

QWidget *DiskToolsPage::buildActionBar()
{
    auto *frame = new QFrame(this);
    frame->setObjectName("actionBarFrame");
    auto *layout = new QHBoxLayout(frame);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(12);

    mLblSelection = new QLabel(tr("No files selected"), frame);
    mLblSelection->setObjectName("lblSelection");
    layout->addWidget(mLblSelection);

    layout->addStretch();

    mBtnTrash = new QPushButton(tr("Move to Trash"), frame);
    mBtnTrash->setObjectName("btnTrash");
    mBtnTrash->setCursor(Qt::PointingHandCursor);
    mBtnTrash->setEnabled(false);
    layout->addWidget(mBtnTrash);

    return frame;
}

// ---- Large & Old Files Mode ----

void DiskToolsPage::buildLargeOldPage()
{
    auto *layout = new QVBoxLayout(ui->pageLargeOld);
    layout->setContentsMargins(0, 8, 0, 0);
    layout->setSpacing(8);

    // Directory picker
    layout->addWidget(buildDirectoryPicker());

    // Filter row
    auto *filterLayout = new QHBoxLayout();
    filterLayout->setSpacing(8);

    filterLayout->addWidget(new QLabel(tr("Size >="), this));
    mSpinSize = new QSpinBox(this);
    mSpinSize->setRange(1, 99999);
    mSpinSize->setValue(100);
    filterLayout->addWidget(mSpinSize);

    mCbSizeUnit = new QComboBox(this);
    mCbSizeUnit->addItems({"MB", "GB"});
    filterLayout->addWidget(mCbSizeUnit);

    filterLayout->addSpacing(16);

    filterLayout->addWidget(new QLabel(tr("Not accessed in >="), this));
    mSpinAge = new QSpinBox(this);
    mSpinAge->setRange(1, 99999);
    mSpinAge->setValue(180);
    filterLayout->addWidget(mSpinAge);

    mCbAgeUnit = new QComboBox(this);
    mCbAgeUnit->addItems({tr("days"), tr("months"), tr("years")});
    filterLayout->addWidget(mCbAgeUnit);

    filterLayout->addSpacing(16);

    filterLayout->addWidget(new QLabel(tr("Match:"), this));
    mCbFilterMode = new QComboBox(this);
    mCbFilterMode->addItems({tr("Either"), tr("Large only"), tr("Old only")});
    filterLayout->addWidget(mCbFilterMode);

    filterLayout->addStretch();

    mBtnLargeOldScan = new QPushButton(tr("Scan"), this);
    mBtnLargeOldScan->setObjectName("btnScan");
    mBtnLargeOldScan->setCursor(Qt::PointingHandCursor);
    connect(mBtnLargeOldScan, &QPushButton::clicked, this, &DiskToolsPage::onLargeOldScan);
    filterLayout->addWidget(mBtnLargeOldScan);

    layout->addLayout(filterLayout);

    // Status label
    mLblLargeOldStatus = new QLabel(this);
    mLblLargeOldStatus->setObjectName("lblStatus");
    layout->addWidget(mLblLargeOldStatus);

    // Results tree
    mTreeLargeOld = new QTreeWidget(this);
    mTreeLargeOld->setObjectName("treeLargeOld");
    mTreeLargeOld->setHeaderLabels({tr("Name"), tr("Path"), tr("Size"),
                                     tr("Last Accessed"), tr("Last Modified")});
    mTreeLargeOld->setRootIsDecorated(false);
    mTreeLargeOld->setSortingEnabled(true);
    mTreeLargeOld->setAlternatingRowColors(true);
    mTreeLargeOld->setSelectionMode(QAbstractItemView::ExtendedSelection);
    mTreeLargeOld->header()->setStretchLastSection(true);
    mTreeLargeOld->header()->setSectionResizeMode(0, QHeaderView::Interactive);
    mTreeLargeOld->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    mTreeLargeOld->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    mTreeLargeOld->header()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    mTreeLargeOld->header()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    connect(mTreeLargeOld, &QTreeWidget::itemChanged, this, [this]() {
        updateSelectionLabel();
    });
    layout->addWidget(mTreeLargeOld, 1);

    // Action bar
    auto *actionBar = buildActionBar();
    connect(mBtnTrash, &QPushButton::clicked, this, &DiskToolsPage::onLargeOldTrash);
    layout->addWidget(actionBar);
}

void DiskToolsPage::onLargeOldScan()
{
    if (mLargeOldScanInProgress || mDirList->count() == 0)
        return;

    mLargeOldScanInProgress = true;
    mBtnLargeOldScan->setEnabled(false);
    mTreeLargeOld->clear();
    mLblLargeOldStatus->setText(tr("Scanning..."));
    mLargeOldResults.clear();

    // Capture filter values on main thread
    qint64 sizeThreshold = mSpinSize->value();
    if (mCbSizeUnit->currentIndex() == 0) // MB
        sizeThreshold *= 1024 * 1024;
    else // GB
        sizeThreshold *= 1024LL * 1024 * 1024;

    int ageValue = mSpinAge->value();
    int ageUnitIdx = mCbAgeUnit->currentIndex();
    // Convert to minutes for find's -amin
    int ageMinutes = ageValue;
    if (ageUnitIdx == 0) ageMinutes = ageValue * 24 * 60;       // days
    else if (ageUnitIdx == 1) ageMinutes = ageValue * 30 * 24 * 60; // months
    else ageMinutes = ageValue * 365 * 24 * 60;                 // years

    int filterMode = mCbFilterMode->currentIndex(); // 0=either, 1=large, 2=old

    QStringList dirs;
    for (int i = 0; i < mDirList->count(); ++i)
        dirs.append(mDirList->item(i)->text());

    mLargeOldFuture = QtConcurrent::run([this, dirs, sizeThreshold, ageMinutes, filterMode]() {
        QList<QFileInfo> results;

        for (const QString &dir : dirs) {
            QDirIterator it(dir, QDir::Files | QDir::NoDotAndDotDot,
                            QDirIterator::Subdirectories);
            while (it.hasNext()) {
                it.next();
                QFileInfo info = it.fileInfo();

                bool isLarge = info.size() >= sizeThreshold;

                QDateTime accessTime = info.lastRead();
                int minutesAgo = accessTime.secsTo(QDateTime::currentDateTime()) / 60;
                bool isOld = minutesAgo >= ageMinutes;

                bool matches = false;
                if (filterMode == 0) matches = isLarge || isOld;
                else if (filterMode == 1) matches = isLarge;
                else matches = isOld;

                if (matches)
                    results.append(info);
            }
        }

        // Sort by size descending
        std::sort(results.begin(), results.end(), [](const QFileInfo &a, const QFileInfo &b) {
            return a.size() > b.size();
        });

        mLargeOldResults = results;
        emit largeOldScanFinishedS();
    });
}

void DiskToolsPage::onLargeOldScanFinished()
{
    mTreeLargeOld->setUpdatesEnabled(false);

    for (const QFileInfo &fi : mLargeOldResults) {
        auto *item = new QTreeWidgetItem(mTreeLargeOld);
        item->setCheckState(0, Qt::Unchecked);
        item->setText(0, fi.fileName());
        item->setText(1, fi.absolutePath());
        item->setText(2, FormatUtil::formatBytes(fi.size()));
        item->setData(2, Qt::UserRole, fi.size()); // For sorting
        item->setText(3, fi.lastRead().toString("yyyy-MM-dd hh:mm"));
        item->setText(4, fi.lastModified().toString("yyyy-MM-dd hh:mm"));
        item->setData(0, Qt::UserRole, fi.absoluteFilePath()); // Store full path
    }

    mTreeLargeOld->setUpdatesEnabled(true);
    mLblLargeOldStatus->setText(tr("%1 files found").arg(mLargeOldResults.size()));
    mBtnLargeOldScan->setEnabled(true);
    mLargeOldScanInProgress = false;
    updateSelectionLabel();
}

void DiskToolsPage::onLargeOldTrash()
{
    QStringList filesToTrash;
    for (int i = 0; i < mTreeLargeOld->topLevelItemCount(); ++i) {
        auto *item = mTreeLargeOld->topLevelItem(i);
        if (item->checkState(0) == Qt::Checked)
            filesToTrash.append(item->data(0, Qt::UserRole).toString());
    }

    if (filesToTrash.isEmpty())
        return;

    quint64 totalSize = selectedSize();
    int count = filesToTrash.size();

    auto reply = QMessageBox::question(this, tr("Move to Trash"),
        tr("Move %1 files (%2) to trash?").arg(count).arg(FormatUtil::formatBytes(totalSize)),
        QMessageBox::Yes | QMessageBox::No);

    if (reply != QMessageBox::Yes)
        return;

    int trashed = 0;
    for (const QString &path : filesToTrash) {
        if (QFile::moveToTrash(path))
            trashed++;
    }

    // Remove trashed items from tree (reverse order)
    for (int i = mTreeLargeOld->topLevelItemCount() - 1; i >= 0; --i) {
        auto *item = mTreeLargeOld->topLevelItem(i);
        if (item->checkState(0) == Qt::Checked) {
            QString path = item->data(0, Qt::UserRole).toString();
            if (!QFile::exists(path))
                delete mTreeLargeOld->takeTopLevelItem(i);
        }
    }

    mLblLargeOldStatus->setText(tr("Moved %1 files to trash").arg(trashed));
    updateSelectionLabel();
}

// ---- Duplicate Finder Mode ----

void DiskToolsPage::buildDuplicatePage()
{
    auto *layout = new QVBoxLayout(ui->pageDuplicates);
    layout->setContentsMargins(0, 8, 0, 0);
    layout->setSpacing(8);

    // Note: directory picker is shared — we reuse the same mDirList
    // The picker widget was already added to the Large & Old page.
    // For duplicates, we reference the same directory list data.

    // Filter row
    auto *filterLayout = new QHBoxLayout();
    filterLayout->setSpacing(8);

    filterLayout->addWidget(new QLabel(tr("Min file size:"), this));
    mSpinMinDupSize = new QSpinBox(this);
    mSpinMinDupSize->setRange(1, 99999);
    mSpinMinDupSize->setValue(1);
    filterLayout->addWidget(mSpinMinDupSize);

    mCbMinDupUnit = new QComboBox(this);
    mCbMinDupUnit->addItems({"KB", "MB", "GB"});
    mCbMinDupUnit->setCurrentIndex(1); // MB
    filterLayout->addWidget(mCbMinDupUnit);

    filterLayout->addSpacing(16);

    filterLayout->addWidget(new QLabel(tr("File pattern:"), this));
    mEditGlob = new QLineEdit(this);
    mEditGlob->setPlaceholderText(tr("e.g., *.jpg or leave empty for all"));
    mEditGlob->setMaximumWidth(200);
    filterLayout->addWidget(mEditGlob);

    filterLayout->addStretch();

    mBtnDupCancel = new QPushButton(tr("Cancel"), this);
    mBtnDupCancel->setObjectName("btnCancel");
    mBtnDupCancel->setCursor(Qt::PointingHandCursor);
    mBtnDupCancel->hide();
    connect(mBtnDupCancel, &QPushButton::clicked, this, [this]() {
        mDupService->cancel();
        mBtnDupCancel->hide();
        mBtnDupScan->show();
    });
    filterLayout->addWidget(mBtnDupCancel);

    mBtnDupScan = new QPushButton(tr("Find Duplicates"), this);
    mBtnDupScan->setObjectName("btnScan");
    mBtnDupScan->setCursor(Qt::PointingHandCursor);
    connect(mBtnDupScan, &QPushButton::clicked, this, &DiskToolsPage::onDupScan);
    filterLayout->addWidget(mBtnDupScan);

    layout->addLayout(filterLayout);

    // Progress bar + status
    mDupProgress = new QProgressBar(this);
    mDupProgress->setObjectName("dupProgress");
    mDupProgress->setTextVisible(false);
    mDupProgress->hide();
    layout->addWidget(mDupProgress);

    mLblDupStatus = new QLabel(this);
    mLblDupStatus->setObjectName("lblStatus");
    layout->addWidget(mLblDupStatus);

    // Results tree (grouped)
    mTreeDuplicates = new QTreeWidget(this);
    mTreeDuplicates->setObjectName("treeDuplicates");
    mTreeDuplicates->setHeaderLabels({tr("Name / Group"), tr("Path"), tr("Size"), tr("Last Modified")});
    mTreeDuplicates->setRootIsDecorated(true);
    mTreeDuplicates->setSortingEnabled(false);
    mTreeDuplicates->setAlternatingRowColors(true);
    mTreeDuplicates->header()->setStretchLastSection(true);
    mTreeDuplicates->header()->setSectionResizeMode(0, QHeaderView::Interactive);
    mTreeDuplicates->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    mTreeDuplicates->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    mTreeDuplicates->header()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    connect(mTreeDuplicates, &QTreeWidget::itemChanged, this, [this]() {
        updateSelectionLabel();
    });
    layout->addWidget(mTreeDuplicates, 1);

    // Action bar (duplicates mode uses a separate trash button connection)
    auto *dupActionBar = new QFrame(this);
    dupActionBar->setObjectName("actionBarFrame");
    auto *dupActionLayout = new QHBoxLayout(dupActionBar);
    dupActionLayout->setContentsMargins(8, 8, 8, 8);
    dupActionLayout->setSpacing(12);

    auto *lblDupSelection = new QLabel(tr("No files selected"), dupActionBar);
    lblDupSelection->setObjectName("lblDupSelection");
    dupActionLayout->addWidget(lblDupSelection);
    dupActionLayout->addStretch();

    auto *btnDupTrash = new QPushButton(tr("Move to Trash"), dupActionBar);
    btnDupTrash->setObjectName("btnTrash");
    btnDupTrash->setCursor(Qt::PointingHandCursor);
    btnDupTrash->setEnabled(false);
    connect(btnDupTrash, &QPushButton::clicked, this, &DiskToolsPage::onDupTrash);
    dupActionLayout->addWidget(btnDupTrash);

    // Store references for updateSelectionLabel
    // We'll handle this in updateSelectionLabel by checking current mode
    layout->addWidget(dupActionBar);
}

void DiskToolsPage::onDupScan()
{
    if (mDupService->isScanning() || mDirList->count() == 0)
        return;

    mTreeDuplicates->clear();
    mDupProgress->show();
    mDupProgress->setRange(0, 0); // indeterminate
    mBtnDupScan->hide();
    mBtnDupCancel->show();

    qint64 minSize = mSpinMinDupSize->value();
    int unitIdx = mCbMinDupUnit->currentIndex();
    if (unitIdx == 0) minSize *= 1024;            // KB
    else if (unitIdx == 1) minSize *= 1024 * 1024; // MB
    else minSize *= 1024LL * 1024 * 1024;          // GB

    QString glob = mEditGlob->text().trimmed();

    QStringList dirs;
    for (int i = 0; i < mDirList->count(); ++i)
        dirs.append(mDirList->item(i)->text());

    mLblDupStatus->setText(tr("Starting scan..."));
    mDupService->scan(dirs, minSize, glob);
}

void DiskToolsPage::onDupProgress(int stage, int current, int total, const QString &message)
{
    mLblDupStatus->setText(message);
    if (total > 0) {
        mDupProgress->setRange(0, total);
        mDupProgress->setValue(current);
    } else {
        mDupProgress->setRange(0, 0);
    }
}

void DiskToolsPage::onDupScanFinished(const QList<DuplicateGroup> &results)
{
    mDupProgress->hide();
    mBtnDupCancel->hide();
    mBtnDupScan->show();

    mTreeDuplicates->setUpdatesEnabled(false);

    quint64 totalWasted = 0;

    for (const DuplicateGroup &group : results) {
        quint64 wastedBytes = group.fileSize * (group.files.size() - 1);
        totalWasted += wastedBytes;

        auto *groupItem = new QTreeWidgetItem(mTreeDuplicates);
        groupItem->setText(0, tr("%1 duplicates").arg(group.files.size()));
        groupItem->setText(2, tr("%1 wasted").arg(FormatUtil::formatBytes(wastedBytes)));
        groupItem->setFlags(groupItem->flags() & ~Qt::ItemIsUserCheckable);

        for (int i = 0; i < group.files.size(); ++i) {
            const QFileInfo &fi = group.files[i];
            auto *child = new QTreeWidgetItem(groupItem);
            child->setCheckState(0, (i == 0) ? Qt::Unchecked : Qt::Checked);
            child->setText(0, fi.fileName());
            child->setText(1, fi.absolutePath());
            child->setText(2, FormatUtil::formatBytes(fi.size()));
            child->setText(3, fi.lastModified().toString("yyyy-MM-dd hh:mm"));
            child->setData(0, Qt::UserRole, fi.absoluteFilePath());
        }
    }

    mTreeDuplicates->expandAll();
    mTreeDuplicates->setUpdatesEnabled(true);

    mLblDupStatus->setText(tr("%1 duplicate groups found — %2 wasted space")
        .arg(results.size()).arg(FormatUtil::formatBytes(totalWasted)));

    updateSelectionLabel();
}

void DiskToolsPage::onDupTrash()
{
    QStringList filesToTrash;

    for (int g = 0; g < mTreeDuplicates->topLevelItemCount(); ++g) {
        auto *groupItem = mTreeDuplicates->topLevelItem(g);
        for (int c = 0; c < groupItem->childCount(); ++c) {
            auto *child = groupItem->child(c);
            if (child->checkState(0) == Qt::Checked)
                filesToTrash.append(child->data(0, Qt::UserRole).toString());
        }
    }

    if (filesToTrash.isEmpty())
        return;

    auto reply = QMessageBox::question(this, tr("Move to Trash"),
        tr("Move %1 duplicate files to trash?").arg(filesToTrash.size()),
        QMessageBox::Yes | QMessageBox::No);

    if (reply != QMessageBox::Yes)
        return;

    int trashed = 0;
    for (const QString &path : filesToTrash) {
        if (QFile::moveToTrash(path))
            trashed++;
    }

    // Remove trashed children from tree (reverse order within each group)
    for (int g = mTreeDuplicates->topLevelItemCount() - 1; g >= 0; --g) {
        auto *groupItem = mTreeDuplicates->topLevelItem(g);
        for (int c = groupItem->childCount() - 1; c >= 0; --c) {
            auto *child = groupItem->child(c);
            if (child->checkState(0) == Qt::Checked) {
                QString path = child->data(0, Qt::UserRole).toString();
                if (!QFile::exists(path))
                    delete groupItem->takeChild(c);
            }
        }
        // Remove empty groups or groups with only 1 file
        if (groupItem->childCount() <= 1)
            delete mTreeDuplicates->takeTopLevelItem(g);
    }

    mLblDupStatus->setText(tr("Moved %1 files to trash").arg(trashed));
    updateSelectionLabel();
}

// ---- Shared helpers ----

void DiskToolsPage::updateSelectionLabel()
{
    int mode = ui->stackedModes->currentIndex();

    if (mode == 0) {
        // Large & Old Files mode
        int count = 0;
        quint64 size = 0;
        for (int i = 0; i < mTreeLargeOld->topLevelItemCount(); ++i) {
            auto *item = mTreeLargeOld->topLevelItem(i);
            if (item->checkState(0) == Qt::Checked) {
                count++;
                size += item->data(2, Qt::UserRole).toULongLong();
            }
        }
        if (count > 0) {
            mLblSelection->setText(tr("%1 files selected (%2)")
                .arg(count).arg(FormatUtil::formatBytes(size)));
            mBtnTrash->setEnabled(true);
        } else {
            mLblSelection->setText(tr("No files selected"));
            mBtnTrash->setEnabled(false);
        }
    } else {
        // Duplicate Finder mode — find the dup action bar labels
        auto *lblDupSelection = ui->pageDuplicates->findChild<QLabel*>("lblDupSelection");
        auto *btnDupTrash = ui->pageDuplicates->findChild<QPushButton*>("btnTrash");
        if (!lblDupSelection || !btnDupTrash)
            return;

        int count = 0;
        quint64 size = 0;
        for (int g = 0; g < mTreeDuplicates->topLevelItemCount(); ++g) {
            auto *groupItem = mTreeDuplicates->topLevelItem(g);
            for (int c = 0; c < groupItem->childCount(); ++c) {
                auto *child = groupItem->child(c);
                if (child->checkState(0) == Qt::Checked) {
                    count++;
                    // Size from the file info stored in column 2 UserRole
                    // We stored formatted text, need raw size
                    QFileInfo fi(child->data(0, Qt::UserRole).toString());
                    size += fi.size();
                }
            }
        }

        if (count > 0) {
            lblDupSelection->setText(tr("%1 files selected (%2)")
                .arg(count).arg(FormatUtil::formatBytes(size)));
            btnDupTrash->setEnabled(true);
        } else {
            lblDupSelection->setText(tr("No files selected"));
            btnDupTrash->setEnabled(false);
        }
    }
}

quint64 DiskToolsPage::selectedSize() const
{
    quint64 size = 0;
    for (int i = 0; i < mTreeLargeOld->topLevelItemCount(); ++i) {
        auto *item = mTreeLargeOld->topLevelItem(i);
        if (item->checkState(0) == Qt::Checked)
            size += item->data(2, Qt::UserRole).toULongLong();
    }
    return size;
}

int DiskToolsPage::selectedCount() const
{
    int count = 0;
    for (int i = 0; i < mTreeLargeOld->topLevelItemCount(); ++i) {
        if (mTreeLargeOld->topLevelItem(i)->checkState(0) == Qt::Checked)
            count++;
    }
    return count;
}

QTreeWidget *DiskToolsPage::currentTree() const
{
    return (ui->stackedModes->currentIndex() == 0) ? mTreeLargeOld : mTreeDuplicates;
}

void DiskToolsPage::refreshThemeColors()
{
    // Theme-aware styling is handled by the global QSS.
    // Add any widget-specific theme updates here if needed.
}
```

**Step 4: Register in CMakeLists.txt**

Add to `GUI_SHARED_SRCS` (after the SystemCleaner entries, around line 270):
```cmake
  "${GUI_SHARED_DIR}/Pages/DiskTools/disk_tools_page.cpp"
```

Add to `GUI_SHARED_HDRS` (after the SystemCleaner entries, around line 337):
```cmake
  "${GUI_SHARED_DIR}/Pages/DiskTools/disk_tools_page.h"
```

Add to `CMAKE_AUTOUIC_SEARCH_PATHS` (after SystemCleaner, around line 408):
```cmake
  "${GUI_SHARED_DIR}/Pages/DiskTools"
```

Add to `target_include_directories` (after SystemCleaner, around line 445):
```cmake
  "${GUI_SHARED_DIR}/Pages/DiskTools"
```

**Step 5: Build to verify page compiles**

Run: `cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=$(brew --prefix qt@6) && cmake --build build -j$(sysctl -n hw.ncpu) 2>&1 | tail -10`
Expected: Build succeeds (page exists but isn't wired into sidebar yet)

**Step 6: Commit**

```bash
git add shared/nexis/Pages/DiskTools/disk_tools_page.h \
        shared/nexis/Pages/DiskTools/disk_tools_page.cpp \
        shared/nexis/Pages/DiskTools/disk_tools_page.ui \
        CMakeLists.txt
git commit -m "feat(disk-tools): add DiskToolsPage skeleton with Large/Old and Duplicate modes (FR-62, FR-63)"
```

---

## Task 4: Wire DiskToolsPage into App Sidebar

**Files:**
- Modify: `shared/nexis/app.h`
- Modify: `shared/nexis/app.cpp`

**Step 1: Add include and member to app.h**

In `shared/nexis/app.h`:

After line 29 (`#include "Pages/Search/search_page.h"`), add:
```cpp
#include "Pages/DiskTools/disk_tools_page.h"
```

After line 86 (`SystemCleanerPage *systemCleanerPage;`), add:
```cpp
    DiskToolsPage *diskToolsPage;
```

After line 126 (`QPushButton *btnSystemCleaner;`), add:
```cpp
    QPushButton *btnDiskTools;
```

**Step 2: Add sidebar button in buildSidebar()**

In `shared/nexis/app.cpp`, in `buildSidebar()`, after line 147 (`mSidebarLayout->addWidget(btnSystemCleaner);`), add:

```cpp
    btnDiskTools = createSidebarButton(tr("Disk Tools"));
    mSidebarLayout->addWidget(btnDiskTools);
```

**Step 3: Add page instantiation in init()**

In `init()`, after line 264 (`systemCleanerPage = new SystemCleanerPage(mSlidingStacked);`), add:

```cpp
    diskToolsPage = new DiskToolsPage(mSlidingStacked);
```

**Step 4: Add button text in init()**

After line 278 (`btnSystemCleaner->setText(tr("System Cleaner"));`), add:

```cpp
    btnDiskTools->setText(tr("Disk Tools"));
```

**Step 5: Add to mListPages and mListSidebarButtons**

In `mListPages` (line 299-301), insert `diskToolsPage` after `systemCleanerPage`:
```cpp
    mListPages = {
        dashboardPage, hardwareInfoPage, resourcesPage, systemCleanerPage, diskToolsPage, searchPage,
        processPage, servicesPage, startupAppsPage, uninstallerPage, helpersPage, settingsPage
    };
```

In `mListSidebarButtons` (line 304-306), insert `btnDiskTools` after `btnSystemCleaner`:
```cpp
    mListSidebarButtons = {
        btnDash, btnHardwareInfo, btnResources, btnSystemCleaner, btnDiskTools, btnSearch,
        btnProcesses, btnServices, btnStartupApps, btnUninstaller, btnHelpers, btnSettings
    };
```

**Step 6: Add click connection**

After line 382 (`connect(btnSystemCleaner, ...)`), add:

```cpp
    connect(btnDiskTools, &QPushButton::clicked, this, [this]() { pageClick(diskToolsPage); });
```

**Step 7: Add icon in updateSidebarIcons()**

After line 668 (`setIcon(btnSystemCleaner, "cleaner.svg");`), add:

```cpp
    setIcon(btnDiskTools, "disk-tools.svg");
```

**Step 8: Build and verify**

Run: `cmake --build build -j$(sysctl -n hw.ncpu) 2>&1 | tail -5`
Expected: Build succeeds — Disk Tools page is now in the sidebar

**Step 9: Commit**

```bash
git add shared/nexis/app.h shared/nexis/app.cpp
git commit -m "feat(disk-tools): wire DiskToolsPage into sidebar after System Cleaner (FR-62, FR-63)"
```

---

## Task 5: Visual Verification and Polish

**Step 1: Run the app and verify the page appears in the sidebar**

Run: `open build/output/nexis.app` (macOS) or `./build/output/nexis` (Linux)

Verify:
- "Disk Tools" appears in MANAGE section after System Cleaner
- Clicking it shows the page with "Large & Old Files" and "Duplicate Finder" mode buttons
- Mode switching works (toggling between the two stacked pages)
- Directory picker shows default directories (Home, Downloads, Documents)
- Add/Remove directory buttons work
- Filter controls render correctly

**Step 2: Test Large & Old Files scan**

- Set size to 10 MB, age to 30 days, mode to "Either"
- Add ~/Downloads as the only directory
- Click Scan
- Verify results appear sorted by size descending
- Verify checkboxes work and selection label updates

**Step 3: Test Duplicate Finder scan**

- Set min size to 100 KB
- Add a small test directory
- Click "Find Duplicates"
- Verify progress updates in the status label
- Verify results appear grouped with first file unchecked, rest checked

**Step 4: Commit any polish fixes**

```bash
git add -A
git commit -m "fix(disk-tools): visual polish from manual testing (FR-62, FR-63)"
```

---

## Task 6: Update Tracking Files and Documentation

**Files:**
- Modify: `FEATURE_REQUESTS.md` (mark FR-62 and FR-63 as done)
- Modify: `docs/APPLICATION_OVERVIEW.md` (add Disk Tools page)
- Modify: `docs/ARCHITECTURE_REVIEW.md` (note new service)

**Step 1: Update FEATURE_REQUESTS.md**

Change FR-62 from `[ ]` to `[x]` and add resolution note.
Change FR-63 from `[ ]` to `[x]` and add resolution note.

**Step 2: Update APPLICATION_OVERVIEW.md**

Add a "Disk Tools" section under the MANAGE pages describing:
- Two modes: Large & Old Files, Duplicate Finder
- Directory picker with smart defaults
- Move to Trash deletion behavior
- 3-stage duplicate detection pipeline

**Step 3: Update ARCHITECTURE_REVIEW.md**

Add DuplicateFinderService to the services inventory. Note the QtConcurrent + QAtomicInt cancellation pattern.

**Step 4: Archive backlog files**

Move any FR-62/FR-63 research/plan files from `backlog/` to `backlog/Archive/`.

**Step 5: Commit**

```bash
git add FEATURE_REQUESTS.md docs/APPLICATION_OVERVIEW.md docs/ARCHITECTURE_REVIEW.md
git commit -m "docs: update tracking and docs for FR-62 and FR-63 Disk Tools page"
```

**Step 6: Push**

```bash
git push
```
