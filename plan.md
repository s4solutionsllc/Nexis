# FR-23: Disk Usage Analyzer — Implementation Plan

> **Feature Request:** Replace the limited File System graph with a true disk usage analyzer:
> pie chart of capacity usage per device, tree view of folders with % capacity used,
> and drill-down navigation.
>
> **Issue:** [lsimpsonsfdc/Stacer#2](https://github.com/lsimpsonsfdc/Stacer/issues/2)
>
> **Platforms:** Linux + macOS

---

## Table of Contents

1. [Architecture Overview](#1-architecture-overview)
2. [New Files](#2-new-files)
3. [Modified Files](#3-modified-files)
4. [Phase 1 — Core: DiskUsageWorker](#4-phase-1--core-diskusageworker)
5. [Phase 2 — UI: DiskAnalyzerWidget](#5-phase-2--ui-diskanalyzerwidget)
6. [Phase 3 — Integration into ResourcesPage](#6-phase-3--integration-into-resourcespage)
7. [Phase 4 — Theme, Polish, and Edge Cases](#7-phase-4--theme-polish-and-edge-cases)
8. [Build Changes](#8-build-changes)
9. [Testing Plan](#9-testing-plan)

---

## 1. Architecture Overview

The current File System section in `ResourcesPage` is a simple `QPieSeries` showing each
mounted volume's **total size** — not its used/free breakdown. The two combo boxes
(Device, File System Type) let you filter which volumes are shown, but there is no
drill-down into folder-level usage.

**Proposed replacement** (within the same Resources page, same position):

```
┌──────────────────────────────────────────────────────────────┐
│  Disk Usage Analyzer  [Device ▼]  [Analyze]  [↑ Up]         │
├─────────────────────────┬────────────────────────────────────┤
│                         │                                    │
│   QPieChart             │   QTreeWidget                      │
│   (used vs free for     │   (folder tree with size,          │
│    selected device,     │    % bar, drill-down)              │
│    top-N folder         │                                    │
│    breakdown)           │                                    │
│                         │                                    │
├─────────────────────────┴────────────────────────────────────┤
│  Status bar: "Scanning /home..." or "234 items, 48.2 GiB"   │
└──────────────────────────────────────────────────────────────┘
```

**Key design decisions:**

1. **Replace, don't add a new page.** The File System pie chart section at the bottom
   of ResourcesPage becomes the Disk Usage Analyzer. No new sidebar entry needed.
2. **Async scanning.** Directory traversal runs in a background thread via
   `QtConcurrent::run()` — the same pattern used by `SystemCleanerPage`.
3. **Lazy drill-down.** Only the top-level of the selected mount point is scanned
   initially. Expanding a tree node triggers a scan of that folder's children.
4. **Cross-platform.** Uses `QStorageInfo` for volume enumeration and `QDir`/`QFileInfo`
   for directory traversal — both fully cross-platform via Qt.

---

## 2. New Files

| File | Purpose |
|------|---------|
| `shared/stacer-core/Info/disk_usage_worker.h` | Worker class: scans a directory, emits results |
| `shared/stacer-core/Info/disk_usage_worker.cpp` | Implementation of the directory scanner |
| `shared/stacer/Pages/Resources/disk_analyzer_widget.h` | Custom QWidget: pie chart + tree view |
| `shared/stacer/Pages/Resources/disk_analyzer_widget.cpp` | Implementation of the analyzer UI |

No new `.ui` files — the widget is built programmatically (matching the existing
`initDiskPieChart()` pattern and the Homebrew tree widget pattern from FR-22).

---

## 3. Modified Files

| File | Change |
|------|--------|
| `shared/stacer/Pages/Resources/resources_page.h` | Replace disk pie chart members with `DiskAnalyzerWidget*` |
| `shared/stacer/Pages/Resources/resources_page.cpp` | Replace `initDiskPieChart()` / `diskPieSeriesCustomize()` with analyzer init |
| `shared/stacer/Managers/info_manager.h` | Add `scanDirectory()` forwarding method |
| `shared/stacer/Managers/info_manager.cpp` | Implement forwarding |
| `CMakeLists.txt` | Add include path for new files (auto-globbed, so likely no change needed) |
| `FEATURE_REQUESTS.md` | Mark FR-23 as `[~]` in-progress, then `[x]` done |

---

## 4. Phase 1 — Core: DiskUsageWorker

### 4.1 Data Structure

```cpp
// shared/stacer-core/Info/disk_usage_worker.h

#ifndef DISK_USAGE_WORKER_H
#define DISK_USAGE_WORKER_H

#include <QObject>
#include <QDir>
#include <QFileInfo>
#include <QStorageInfo>
#include <QAtomicInt>
#include "stacer-core_global.h"

struct STACERCORESHARED_EXPORT DirEntry {
    QString name;           // Display name (folder or file name)
    QString absolutePath;   // Full path
    quint64 size;           // Total size in bytes (recursive for dirs)
    bool isDir;             // true = directory, false = file
    int childCount;         // Number of direct children (dirs only, -1 if not scanned)
};

class STACERCORESHARED_EXPORT DiskUsageWorker : public QObject
{
    Q_OBJECT

public:
    explicit DiskUsageWorker(QObject *parent = nullptr);

    // Scan immediate children of `path` and compute their sizes.
    // Each child directory's size is computed recursively.
    // Emits scanFinished() when done.
    void scanDirectory(const QString &path);

    // Cancel a running scan
    void cancel();

signals:
    // Emitted when scan completes. Results are the direct children of the
    // scanned path, sorted by size descending.
    void scanFinished(const QString &path, const QList<DirEntry> &entries, quint64 totalSize);

    // Progress updates during scan
    void scanProgress(const QString &currentDir, int itemsScanned);

private:
    quint64 calculateDirSize(const QString &path, int &itemCount);

    QAtomicInt mCancelled;
};

#endif // DISK_USAGE_WORKER_H
```

### 4.2 Implementation

```cpp
// shared/stacer-core/Info/disk_usage_worker.cpp

#include "disk_usage_worker.h"
#include <QDirIterator>
#include <algorithm>

DiskUsageWorker::DiskUsageWorker(QObject *parent)
    : QObject(parent)
{
    mCancelled.storeRelaxed(0);
}

void DiskUsageWorker::cancel()
{
    mCancelled.storeRelaxed(1);
}

void DiskUsageWorker::scanDirectory(const QString &path)
{
    mCancelled.storeRelaxed(0);

    QDir dir(path);
    if (!dir.exists()) {
        emit scanFinished(path, {}, 0);
        return;
    }

    QList<DirEntry> entries;
    quint64 totalSize = 0;
    int itemsScanned = 0;

    // Enumerate immediate children
    const QFileInfoList children = dir.entryInfoList(
        QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden,
        QDir::DirsFirst | QDir::Name);

    for (const QFileInfo &fi : children) {
        if (mCancelled.loadRelaxed())
            return;

        DirEntry entry;
        entry.name = fi.fileName();
        entry.absolutePath = fi.absoluteFilePath();
        entry.isDir = fi.isDir();

        if (fi.isDir()) {
            // Skip symlinks to avoid infinite loops
            if (fi.isSymLink()) {
                entry.size = 0;
                entry.childCount = 0;
            } else {
                int subCount = 0;
                entry.size = calculateDirSize(fi.absoluteFilePath(), subCount);
                entry.childCount = QDir(fi.absoluteFilePath())
                    .entryList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden)
                    .count();
            }
        } else {
            entry.size = fi.size();
            entry.childCount = -1; // not a directory
        }

        totalSize += entry.size;
        entries.append(entry);

        itemsScanned++;
        if (itemsScanned % 50 == 0) {
            emit scanProgress(fi.absoluteFilePath(), itemsScanned);
        }
    }

    // Sort by size descending
    std::sort(entries.begin(), entries.end(),
              [](const DirEntry &a, const DirEntry &b) {
                  return a.size > b.size;
              });

    emit scanFinished(path, entries, totalSize);
}

quint64 DiskUsageWorker::calculateDirSize(const QString &path, int &itemCount)
{
    quint64 total = 0;

    QDirIterator it(path,
                    QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden,
                    QDirIterator::Subdirectories);

    while (it.hasNext()) {
        if (mCancelled.loadRelaxed())
            return total;

        it.next();
        const QFileInfo fi = it.fileInfo();

        if (fi.isFile() && !fi.isSymLink()) {
            total += fi.size();
        }
        itemCount++;
    }

    return total;
}
```

**Why `QDirIterator` instead of `FileUtil::getFileSize()`?**
`FileUtil::getFileSize()` uses recursive `QDir::entryInfoList()` which is correct but
slow for deep trees because it builds full `QFileInfo` lists at each level.
`QDirIterator` with `Subdirectories` flag is flat-iterative and significantly faster for
large directory trees. We also need cancellation support, which the existing utility
doesn't provide.

### 4.3 Platform Considerations

| Concern | Linux | macOS | Handling |
|---------|-------|-------|----------|
| Permission errors | `/root`, `/proc` entries | SIP-protected dirs | `QFileInfo::isReadable()` checked; inaccessible dirs report size = 0 |
| Symlink loops | Possible in `/` | Possible (e.g. `/var` → `/private/var`) | `QFileInfo::isSymLink()` skip for size calc |
| Virtual filesystems | `/proc`, `/sys`, `/dev` | `/dev` | Filtered out — only scan real mount points from QStorageInfo |
| Large directories | `/usr` with 100k+ files | Similar | Progress signal + cancellation via `QAtomicInt` |

---

## 5. Phase 2 — UI: DiskAnalyzerWidget

### 5.1 Header

```cpp
// shared/stacer/Pages/Resources/disk_analyzer_widget.h

#ifndef DISK_ANALYZER_WIDGET_H
#define DISK_ANALYZER_WIDGET_H

#include <QWidget>
#include <QTreeWidget>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QProgressBar>
#include <QtCharts>
#include <QFuture>

#include "Managers/info_manager.h"
#include "Info/disk_usage_worker.h"
#include "signal_mapper.h"

class DiskAnalyzerWidget : public QWidget
{
    Q_OBJECT

public:
    explicit DiskAnalyzerWidget(const QList<int> &chartColors, QWidget *parent = nullptr);
    ~DiskAnalyzerWidget();

private slots:
    void onDeviceChanged(int index);
    void onAnalyzeClicked();
    void onNavigateUp();
    void onScanFinished(const QString &path, const QList<DirEntry> &entries, quint64 totalSize);
    void onScanProgress(const QString &currentDir, int itemsScanned);
    void onTreeItemDoubleClicked(QTreeWidgetItem *item, int column);
    void onThemeChanged();

private:
    void buildUi();
    void populateDeviceCombo();
    void updatePieChart(const QList<DirEntry> &entries, quint64 deviceTotal, quint64 deviceFree);
    void populateTree(const QList<DirEntry> &entries);
    void drillDown(const QString &path);
    void updateBreadcrumb();

private:
    InfoManager *mInfoManager;
    DiskUsageWorker *mWorker;
    QFuture<void> mScanFuture;

    // Current state
    QString mCurrentPath;       // Path currently displayed
    QString mMountPoint;        // Root mount point of selected device
    quint64 mDeviceTotal;       // Total bytes of selected device
    quint64 mDeviceFree;        // Free bytes of selected device

    // Toolbar
    QComboBox *mDeviceCombo;
    QPushButton *mAnalyzeBtn;
    QPushButton *mUpBtn;
    QLabel *mBreadcrumb;

    // Pie chart (left panel)
    QChart *mPieChart;
    QChartView *mPieChartView;
    QPieSeries *mPieSeries;

    // Tree (right panel)
    QTreeWidget *mTree;

    // Status bar
    QLabel *mStatusLabel;
    QProgressBar *mProgressBar;

    // Colors (shared with ResourcesPage)
    QList<int> mChartColors;
};

#endif // DISK_ANALYZER_WIDGET_H
```

### 5.2 Implementation

```cpp
// shared/stacer/Pages/Resources/disk_analyzer_widget.cpp

#include "disk_analyzer_widget.h"
#include "Managers/app_manager.h"
#include "utilities.h"
#include <QGridLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QHeaderView>
#include <QtConcurrent>
#include <Utils/format_util.h>

DiskAnalyzerWidget::DiskAnalyzerWidget(const QList<int> &chartColors, QWidget *parent)
    : QWidget(parent)
    , mInfoManager(InfoManager::ins())
    , mWorker(new DiskUsageWorker(this))
    , mDeviceTotal(0)
    , mDeviceFree(0)
    , mChartColors(chartColors)
{
    buildUi();

    connect(mWorker, &DiskUsageWorker::scanFinished,
            this, &DiskAnalyzerWidget::onScanFinished, Qt::QueuedConnection);
    connect(mWorker, &DiskUsageWorker::scanProgress,
            this, &DiskAnalyzerWidget::onScanProgress, Qt::QueuedConnection);
    connect(SignalMapper::ins(), &SignalMapper::sigChangedAppTheme,
            this, &DiskAnalyzerWidget::onThemeChanged);

    populateDeviceCombo();
}

DiskAnalyzerWidget::~DiskAnalyzerWidget()
{
    mWorker->cancel();
    if (mScanFuture.isRunning())
        mScanFuture.waitForFinished();
}

// ─── UI Construction ───────────────────────────────────────────────

void DiskAnalyzerWidget::buildUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(5);

    // ── Toolbar row ──
    QHBoxLayout *toolbar = new QHBoxLayout;

    QLabel *title = new QLabel(tr("Disk Usage Analyzer"), this);
    title->setObjectName("lblHistoryTitle");

    mDeviceCombo = new QComboBox(this);
    mAnalyzeBtn = new QPushButton(tr("Analyze"), this);
    mAnalyzeBtn->setCursor(Qt::PointingHandCursor);
    mUpBtn = new QPushButton(tr("↑ Up"), this);
    mUpBtn->setCursor(Qt::PointingHandCursor);
    mUpBtn->setEnabled(false);

    mBreadcrumb = new QLabel(this);
    mBreadcrumb->setWordWrap(true);

    toolbar->addWidget(title);
    toolbar->addWidget(mDeviceCombo);
    toolbar->addWidget(mAnalyzeBtn);
    toolbar->addWidget(mUpBtn);
    toolbar->addStretch();
    toolbar->addWidget(mBreadcrumb);

    mainLayout->addLayout(toolbar);

    // ── Content: Splitter with pie chart (left) + tree (right) ──
    QSplitter *splitter = new QSplitter(Qt::Horizontal, this);

    // Pie chart
    mPieSeries = new QPieSeries();
    mPieChart = new QChart();
    mPieChart->addSeries(mPieSeries);
    mPieChart->legend()->hide();
    mPieChart->setAnimationOptions(QChart::AllAnimations);
    mPieChart->setContentsMargins(-11, -11, -11, -11);
    mPieChart->setMargins(QMargins(10, 10, 10, 10));

    mPieChartView = new QChartView(mPieChart);
    mPieChartView->setRenderHint(QPainter::Antialiasing);
    mPieChartView->setMinimumWidth(300);
    mPieChartView->setMinimumHeight(350);

    // Tree widget
    mTree = new QTreeWidget(this);
    mTree->setColumnCount(3);
    mTree->setHeaderLabels({tr("Name"), tr("Size"), tr("% of Parent")});
    mTree->header()->setStretchLastSection(false);
    mTree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    mTree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    mTree->header()->setSectionResizeMode(2, QHeaderView::Fixed);
    mTree->header()->resizeSection(2, 120);
    mTree->setAlternatingRowColors(true);
    mTree->setSortingEnabled(true);
    mTree->sortByColumn(1, Qt::DescendingOrder);
    mTree->setMinimumWidth(400);

    splitter->addWidget(mPieChartView);
    splitter->addWidget(mTree);
    splitter->setStretchFactor(0, 1); // pie chart gets 1/3
    splitter->setStretchFactor(1, 2); // tree gets 2/3

    mainLayout->addWidget(splitter, 1);

    // ── Status bar ──
    QHBoxLayout *statusBar = new QHBoxLayout;
    mStatusLabel = new QLabel(tr("Select a device and click Analyze."), this);
    mProgressBar = new QProgressBar(this);
    mProgressBar->setRange(0, 0); // indeterminate
    mProgressBar->setMaximumWidth(200);
    mProgressBar->hide();

    statusBar->addWidget(mStatusLabel);
    statusBar->addStretch();
    statusBar->addWidget(mProgressBar);

    mainLayout->addLayout(statusBar);

    // ── Signals ──
    connect(mDeviceCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &DiskAnalyzerWidget::onDeviceChanged);
    connect(mAnalyzeBtn, &QPushButton::clicked,
            this, &DiskAnalyzerWidget::onAnalyzeClicked);
    connect(mUpBtn, &QPushButton::clicked,
            this, &DiskAnalyzerWidget::onNavigateUp);
    connect(mTree, &QTreeWidget::itemDoubleClicked,
            this, &DiskAnalyzerWidget::onTreeItemDoubleClicked);
}

// ─── Device Selection ──────────────────────────────────────────────

void DiskAnalyzerWidget::populateDeviceCombo()
{
    mDeviceCombo->clear();

    for (const Disk *disk : mInfoManager->getDisks()) {
        // Filter out tiny/virtual filesystems (< 1 MiB)
        if (disk->size < FormatUtil::MEBI)
            continue;

        QString label = QString("%1 (%2) — %3 / %4")
            .arg(disk->name)
            .arg(disk->device)
            .arg(FormatUtil::formatBytes(disk->used))
            .arg(FormatUtil::formatBytes(disk->size));

        // Store mount point path as userData
        // QStorageInfo gives us mount points cross-platform
        QStorageInfo si(disk->device);
        // Find the matching QStorageInfo by device to get rootPath
        for (const QStorageInfo &vol : QStorageInfo::mountedVolumes()) {
            if (vol.device() == disk->device.toUtf8() && vol.isValid()) {
                mDeviceCombo->addItem(label, vol.rootPath());
                break;
            }
        }
    }
}

void DiskAnalyzerWidget::onDeviceChanged(int index)
{
    if (index < 0) return;

    QString mountPoint = mDeviceCombo->currentData().toString();
    mMountPoint = mountPoint;

    // Find corresponding Disk to get total/free
    for (const Disk *disk : mInfoManager->getDisks()) {
        for (const QStorageInfo &vol : QStorageInfo::mountedVolumes()) {
            if (vol.device() == disk->device.toUtf8()
                && vol.rootPath() == mountPoint) {
                mDeviceTotal = disk->size;
                mDeviceFree = disk->free;
                break;
            }
        }
    }

    mStatusLabel->setText(tr("Ready. Click Analyze to scan %1").arg(mountPoint));
    mUpBtn->setEnabled(false);
}

// ─── Scanning ──────────────────────────────────────────────────────

void DiskAnalyzerWidget::onAnalyzeClicked()
{
    if (mDeviceCombo->currentIndex() < 0)
        return;

    // Cancel any running scan
    mWorker->cancel();
    if (mScanFuture.isRunning())
        mScanFuture.waitForFinished();

    QString path = mDeviceCombo->currentData().toString();
    drillDown(path);
}

void DiskAnalyzerWidget::drillDown(const QString &path)
{
    mCurrentPath = path;
    updateBreadcrumb();

    mUpBtn->setEnabled(mCurrentPath != mMountPoint);
    mAnalyzeBtn->setEnabled(false);
    mTree->clear();
    mProgressBar->show();
    mStatusLabel->setText(tr("Scanning %1...").arg(path));

    // Run scan in background thread
    DiskUsageWorker *worker = mWorker;
    mScanFuture = QtConcurrent::run([worker, path]() {
        worker->scanDirectory(path);
    });
}

void DiskAnalyzerWidget::onNavigateUp()
{
    if (mCurrentPath == mMountPoint)
        return;

    QDir dir(mCurrentPath);
    if (dir.cdUp()) {
        // Don't go above mount point
        QString parentPath = dir.absolutePath();
        if (!parentPath.startsWith(mMountPoint)) {
            parentPath = mMountPoint;
        }
        drillDown(parentPath);
    }
}

void DiskAnalyzerWidget::onScanFinished(const QString &path,
                                         const QList<DirEntry> &entries,
                                         quint64 totalSize)
{
    mProgressBar->hide();
    mAnalyzeBtn->setEnabled(true);

    int dirCount = 0, fileCount = 0;
    for (const DirEntry &e : entries) {
        if (e.isDir) dirCount++;
        else fileCount++;
    }

    mStatusLabel->setText(tr("%1 folders, %2 files — %3")
                          .arg(dirCount)
                          .arg(fileCount)
                          .arg(FormatUtil::formatBytes(totalSize)));

    updatePieChart(entries, mDeviceTotal, mDeviceFree);
    populateTree(entries);
}

void DiskAnalyzerWidget::onScanProgress(const QString &currentDir, int itemsScanned)
{
    mStatusLabel->setText(tr("Scanning... %1 items (%2)")
                          .arg(itemsScanned)
                          .arg(currentDir.section('/', -1)));
}

// ─── Pie Chart ─────────────────────────────────────────────────────

void DiskAnalyzerWidget::updatePieChart(const QList<DirEntry> &entries,
                                         quint64 deviceTotal,
                                         quint64 deviceFree)
{
    mPieChart->removeSeries(mPieSeries);
    mPieSeries = new QPieSeries();

    // Show top N largest entries in pie, group rest as "Other"
    const int maxSlices = 8;
    quint64 shownSize = 0;

    int count = qMin(maxSlices, static_cast<int>(entries.size()));
    for (int i = 0; i < count; ++i) {
        const DirEntry &e = entries.at(i);
        if (e.size == 0) continue;

        QString label = e.name;
        mPieSeries->append(label, e.size);
        shownSize += e.size;
    }

    // "Other" slice for remaining entries
    quint64 otherSize = 0;
    for (int i = maxSlices; i < entries.size(); ++i) {
        otherSize += entries.at(i).size;
    }
    if (otherSize > 0) {
        mPieSeries->append(tr("Other"), otherSize);
        shownSize += otherSize;
    }

    // "Free space" slice (only at mount-point level)
    if (mCurrentPath == mMountPoint && deviceFree > 0) {
        mPieSeries->append(tr("Free"), deviceFree);
    }

    // Style slices
    for (int i = 0; i < mPieSeries->count(); ++i) {
        QPieSlice *slice = mPieSeries->slices().at(i);

        int colorIdx = i % mChartColors.count();
        slice->setBrush(QColor(static_cast<QRgb>(mChartColors.at(colorIdx))));
        slice->setBorderColor(QColor(Qt::lightGray));

        // Hover: explode + show details in chart title
        connect(slice, &QPieSlice::hovered, this, [this, slice](bool show) {
            slice->setExploded(show);
            if (show) {
                mPieChart->setTitle(QString("%1 — %2 (%3)")
                    .arg(slice->label())
                    .arg(FormatUtil::formatBytes(slice->value()))
                    .arg(QString::asprintf("%.1f%%", slice->percentage() * 100)));
            } else {
                mPieChart->setTitle("");
            }
        });
    }

    mPieChart->addSeries(mPieSeries);

    // Re-apply theme colors
    onThemeChanged();
}

// ─── Tree Widget ───────────────────────────────────────────────────

void DiskAnalyzerWidget::populateTree(const QList<DirEntry> &entries)
{
    mTree->setSortingEnabled(false);
    mTree->clear();

    quint64 parentTotal = 0;
    for (const DirEntry &e : entries)
        parentTotal += e.size;

    for (const DirEntry &e : entries) {
        QTreeWidgetItem *item = new QTreeWidgetItem(mTree);

        // Column 0: Name (with folder/file icon)
        item->setText(0, e.name);
        if (e.isDir) {
            item->setIcon(0, QIcon::fromTheme("folder",
                QIcon(":/static/themes/common/img/c_cache.svg")));
        } else {
            item->setIcon(0, QIcon::fromTheme("text-x-generic",
                QIcon(":/static/themes/common/img/c_logs.svg")));
        }

        // Column 1: Size (sortable via data role)
        item->setText(1, FormatUtil::formatBytes(e.size));
        item->setData(1, Qt::UserRole, e.size); // for sorting

        // Column 2: Percentage bar (text + visual)
        double pct = parentTotal > 0 ? (double)e.size / (double)parentTotal * 100.0 : 0.0;
        item->setText(2, QString::asprintf("%.1f%%", pct));
        item->setData(2, Qt::UserRole, pct); // for sorting

        // Store metadata for drill-down
        item->setData(0, Qt::UserRole, e.absolutePath);
        item->setData(0, Qt::UserRole + 1, e.isDir);
        item->setData(0, Qt::UserRole + 2, e.childCount);
    }

    mTree->setSortingEnabled(true);
    mTree->sortByColumn(1, Qt::DescendingOrder);
}

void DiskAnalyzerWidget::onTreeItemDoubleClicked(QTreeWidgetItem *item, int column)
{
    Q_UNUSED(column);

    bool isDir = item->data(0, Qt::UserRole + 1).toBool();
    if (!isDir) return;

    int childCount = item->data(0, Qt::UserRole + 2).toInt();
    if (childCount == 0) return; // empty directory

    QString path = item->data(0, Qt::UserRole).toString();
    drillDown(path);
}

// ─── Breadcrumb ────────────────────────────────────────────────────

void DiskAnalyzerWidget::updateBreadcrumb()
{
    // Show path relative to mount point
    QString rel = mCurrentPath;
    if (mCurrentPath.startsWith(mMountPoint)) {
        rel = mCurrentPath.mid(mMountPoint.length());
        if (rel.isEmpty()) rel = "/";
    }
    mBreadcrumb->setText(rel);
}

// ─── Theme ─────────────────────────────────────────────────────────

void DiskAnalyzerWidget::onThemeChanged()
{
    auto *styles = AppManager::ins()->getStyleValues();
    QString chartLabelColor = styles->value("@chartLabelColor").toString();
    QString chartGridColor = styles->value("@chartGridColor").toString();
    QString bgColor = styles->value("@historyChartBackgroundColor").toString();

    mPieChart->setBackgroundBrush(QColor(bgColor));
    mPieChart->setTitleBrush(QColor(chartGridColor));

    for (int i = 0; i < mPieSeries->count(); ++i) {
        mPieSeries->slices().at(i)->setLabelBrush(QColor(chartGridColor));
    }
}
```

### 5.3 Custom Sort for Tree

The tree needs to sort by raw byte count (column 1) and percentage (column 2), not
by display text. We extend the same approach used by `ByteTreeWidget`:

```cpp
// Inside disk_analyzer_widget.cpp — custom sort support

// Override for the tree: create a subclass or use a QTreeWidgetItem subclass
class DiskTreeItem : public QTreeWidgetItem
{
public:
    using QTreeWidgetItem::QTreeWidgetItem;

    bool operator<(const QTreeWidgetItem &other) const override
    {
        int col = treeWidget()->sortColumn();
        if (col == 1) {
            // Sort by raw byte count
            return data(1, Qt::UserRole).toULongLong()
                 < other.data(1, Qt::UserRole).toULongLong();
        }
        if (col == 2) {
            // Sort by percentage
            return data(2, Qt::UserRole).toDouble()
                 < other.data(2, Qt::UserRole).toDouble();
        }
        return text(col).toLower() < other.text(col).toLower();
    }
};
```

Then in `populateTree()`, replace `new QTreeWidgetItem(mTree)` with
`new DiskTreeItem(mTree)`.

---

## 6. Phase 3 — Integration into ResourcesPage

### 6.1 Header Changes

```cpp
// shared/stacer/Pages/Resources/resources_page.h

// REMOVE these members:
//   QChartView *mChartViewDiskPie;
//   QChart *mChartDiskPie;
//   QWidget *gridWidgetDiskPie;
//   QGridLayout *gridLayoutDiskPie;
//   QPieSeries *mDiskPieSeries;

// REMOVE these slots:
//   void initDiskPieChart();
//   void diskPieSeriesCustomize();

// ADD:
#include "disk_analyzer_widget.h"

// ADD member:
    DiskAnalyzerWidget *mDiskAnalyzer;
```

### 6.2 Source Changes

In `resources_page.cpp`, the `init()` method currently ends with:

```cpp
initDiskPieChart();
```

**Replace with:**

```cpp
// Disk Usage Analyzer (replaces old disk pie chart)
mDiskAnalyzer = new DiskAnalyzerWidget(chartColors, this);
ui->chartsLayout->addWidget(mDiskAnalyzer);
Utilities::addDropShadow(mDiskAnalyzer, 40);
```

**Remove entirely:**
- `initDiskPieChart()` method
- `diskPieSeriesCustomize()` method
- All `mChartDiskPie`, `mDiskPieSeries`, `gridWidgetDiskPie`, `gridLayoutDiskPie` references

The "check to hide other charts" feature from the old implementation can be preserved
by connecting a checkbox in `DiskAnalyzerWidget` if desired — but it's simpler to drop
it since the analyzer is more visually prominent and less likely to need full-screen mode.

---

## 7. Phase 4 — Theme, Polish, and Edge Cases

### 7.1 Permission Handling

On both Linux and macOS, some directories are not readable without root. The scanner
should handle this gracefully:

```cpp
// In DiskUsageWorker::scanDirectory(), before enumerating children:
QFileInfo dirInfo(path);
if (!dirInfo.isReadable()) {
    emit scanFinished(path, {}, 0);
    return;
}
```

And in `calculateDirSize()`, `QDirIterator` already silently skips unreadable entries.

### 7.2 Virtual Filesystem Filtering

When populating the device combo, filter out pseudo-filesystems:

```cpp
void DiskAnalyzerWidget::populateDeviceCombo()
{
    static const QStringList excludedFsTypes = {
        "proc", "sysfs", "devtmpfs", "tmpfs", "devpts",
        "securityfs", "cgroup", "cgroup2", "autofs",
        "debugfs", "tracefs", "hugetlbfs", "mqueue",
        "fusectl", "configfs", "pstore", "binfmt_misc",
        "efivarfs", "bpf", "ramfs"
    };

    for (const Disk *disk : mInfoManager->getDisks()) {
        if (disk->size < FormatUtil::MEBI)
            continue;
        if (excludedFsTypes.contains(disk->fileSystemType))
            continue;

        // ... add to combo
    }
}
```

### 7.3 Symlink Safety

Already handled: `QFileInfo::isSymLink()` check skips symlinked directories in size
calculation to prevent infinite recursion. In the tree, symlinks are shown with size 0
and cannot be drilled into.

### 7.4 Percentage Bar in Tree Column

For a more visual experience, add a progress-bar delegate for column 2:

```cpp
// In disk_analyzer_widget.h:
#include <QStyledItemDelegate>

class PercentBarDelegate : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override
    {
        // Draw background
        QStyledItemDelegate::paint(painter, option, index);

        double pct = index.data(Qt::UserRole).toDouble();
        if (pct <= 0) return;

        painter->save();

        QRect barRect = option.rect.adjusted(4, 4, -4, -4);
        int fillWidth = static_cast<int>(barRect.width() * pct / 100.0);

        // Bar background
        painter->fillRect(barRect, QColor(60, 60, 60, 40));
        // Bar fill
        painter->fillRect(QRect(barRect.x(), barRect.y(), fillWidth, barRect.height()),
                          QColor(0x2ec27e)); // green from palette

        // Text overlay
        painter->setPen(option.palette.color(QPalette::Text));
        painter->drawText(barRect, Qt::AlignCenter,
                          QString::asprintf("%.1f%%", pct));

        painter->restore();
    }
};
```

Set the delegate in `buildUi()`:

```cpp
mTree->setItemDelegateForColumn(2, new PercentBarDelegate(mTree));
```

---

## 8. Build Changes

The `CMakeLists.txt` uses `file(GLOB_RECURSE ...)` to collect sources from
`shared/stacer-core/` and `shared/stacer/Pages/Resources/`. Since our new files live in
these existing directories, **no CMakeLists.txt changes are required** — the glob will
pick them up automatically.

**Verify after adding files:**

```bash
mkdir -p build && cd build
cmake ..
make -j$(nproc)
```

If the new `disk_usage_worker.h` header isn't found, add to `CMakeLists.txt`:

```cmake
# In the stacer-core include directories (should already be covered by glob):
"${CORE_SHARED_DIR}/Info"
```

This is already present at line 45, so no change should be needed.

---

## 9. Testing Plan

### 9.1 Manual Test Matrix

| Test | Linux | macOS | Expected |
|------|-------|-------|----------|
| Device combo lists all real volumes | ✓ | ✓ | Shows /, /home, etc. / Shows Macintosh HD, etc. |
| Analyze scans root of selected volume | ✓ | ✓ | Tree populates, pie chart updates |
| Pie chart shows top-N + Other + Free | ✓ | ✓ | Correct proportions, hover explodes |
| Double-click folder drills down | ✓ | ✓ | Tree refreshes with child contents |
| "Up" button navigates to parent | ✓ | ✓ | Stops at mount point root |
| Sort by size works | ✓ | ✓ | Largest first by default |
| Sort by name works | ✓ | ✓ | Alphabetical |
| Sort by percentage works | ✓ | ✓ | Highest % first |
| Permission-denied dirs show size 0 | ✓ | ✓ | No crash, graceful fallback |
| Scanning large dir shows progress | ✓ | ✓ | Status label updates |
| Cancel (switch device mid-scan) | ✓ | ✓ | Old scan stops, new one starts |
| Theme switch updates chart colors | ✓ | ✓ | Background, labels, grid adapt |
| Virtual FS excluded from combo | ✓ | N/A | No /proc, /sys, etc. |

### 9.2 Build Verification

```bash
# Full clean build
rm -rf build && mkdir build && cd build
cmake .. && make -j$(nproc)

# Run (Linux)
./output/stacer

# Run (macOS)
open output/stacer.app
# or
./output/stacer.app/Contents/MacOS/stacer
```

### 9.3 Edge Cases to Verify

1. **Empty volume** — newly formatted disk with no files
2. **Single file volume** — e.g., a mounted ISO
3. **Volume with millions of files** — e.g., node_modules deep tree (verify cancellation works)
4. **Root filesystem** — ensure no crashes on permission-denied dirs
5. **External USB drive** — hot-plug after app start, re-click Analyze
6. **Network mount (NFS/SMB)** — may be very slow; verify cancellation

---

## Summary of Changes

| Component | Lines (est.) | Complexity |
|-----------|:------------:|:----------:|
| `disk_usage_worker.h/cpp` | ~120 | Low — straightforward `QDirIterator` |
| `disk_analyzer_widget.h/cpp` | ~350 | Medium — UI composition, chart + tree |
| `resources_page.h/cpp` modifications | ~-80, +10 | Low — remove old, add new widget |
| Total net new | ~400 | |

**No platform-specific code needed.** Everything uses Qt cross-platform APIs
(`QStorageInfo`, `QDir`, `QDirIterator`, `QFileInfo`).
