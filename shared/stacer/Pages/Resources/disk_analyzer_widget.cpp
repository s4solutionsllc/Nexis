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

const QStringList DiskAnalyzerWidget::sExcludedFsTypes = {
    "proc", "sysfs", "devtmpfs", "tmpfs", "devpts",
    "securityfs", "cgroup", "cgroup2", "autofs",
    "debugfs", "tracefs", "hugetlbfs", "mqueue",
    "fusectl", "configfs", "pstore", "binfmt_misc",
    "efivarfs", "bpf", "ramfs"
};

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
    mDeviceCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    mAnalyzeBtn = new QPushButton(tr("Analyze"), this);
    mAnalyzeBtn->setCursor(Qt::PointingHandCursor);
    mAnalyzeBtn->setAccessibleName("primary");

    mUpBtn = new QPushButton(tr("Up"), this);
    mUpBtn->setCursor(Qt::PointingHandCursor);
    mUpBtn->setEnabled(false);
    mUpBtn->setAccessibleName("primary");

    mBreadcrumb = new QLabel(this);
    mBreadcrumb->setWordWrap(true);
    mBreadcrumb->setObjectName("lblBreadcrumb");

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
    mTree->header()->resizeSection(2, 140);
    mTree->setAlternatingRowColors(true);
    mTree->setSortingEnabled(true);
    mTree->sortByColumn(1, Qt::DescendingOrder);
    mTree->setMinimumWidth(400);
    mTree->setItemDelegateForColumn(2, new PercentBarDelegate(mTree));

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
    mInfoManager->updateDiskInfo();

    const QList<QStorageInfo> volumes = QStorageInfo::mountedVolumes();

    for (const Disk *disk : mInfoManager->getDisks()) {
        // Filter out tiny/virtual filesystems (< 1 MiB)
        if (disk->size < FormatUtil::MEBI)
            continue;

        // Filter out pseudo-filesystems
        if (sExcludedFsTypes.contains(disk->fileSystemType))
            continue;

        // Find matching QStorageInfo to get the mount point (rootPath)
        for (const QStorageInfo &vol : volumes) {
            if (vol.device() == disk->device.toUtf8() && vol.isValid()) {
                QString label = QString("%1 (%2) - %3 / %4")
                    .arg(disk->name)
                    .arg(vol.rootPath())
                    .arg(FormatUtil::formatBytes(disk->used))
                    .arg(FormatUtil::formatBytes(disk->size));

                // Store mount point + device + total + free as userData
                QVariantMap data;
                data["mountPoint"] = vol.rootPath();
                data["total"] = disk->size;
                data["free"] = disk->free;

                mDeviceCombo->addItem(label, data);
                break;
            }
        }
    }

    // Select first item if available
    if (mDeviceCombo->count() > 0) {
        onDeviceChanged(0);
    }
}

void DiskAnalyzerWidget::onDeviceChanged(int index)
{
    if (index < 0) return;

    QVariantMap data = mDeviceCombo->currentData().toMap();
    mMountPoint = data["mountPoint"].toString();
    mDeviceTotal = data["total"].toULongLong();
    mDeviceFree = data["free"].toULongLong();

    mStatusLabel->setText(tr("Ready. Click Analyze to scan %1").arg(mMountPoint));
    mUpBtn->setEnabled(false);
    mCurrentPath.clear();
    mBreadcrumb->clear();
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

    drillDown(mMountPoint);
}

void DiskAnalyzerWidget::drillDown(const QString &path)
{
    mCurrentPath = path;
    updateBreadcrumb();

    mUpBtn->setEnabled(mCurrentPath != mMountPoint);
    mAnalyzeBtn->setEnabled(false);
    mTree->clear();
    mProgressBar->show();
    mStatusLabel->setText(tr("Scanning %1 ...").arg(path));

    // Run scan in background thread
    DiskUsageWorker *worker = mWorker;
    mScanFuture = QtConcurrent::run([worker, path]() {
        worker->scanDirectory(path);
    });
}

void DiskAnalyzerWidget::onNavigateUp()
{
    if (mCurrentPath.isEmpty() || mCurrentPath == mMountPoint)
        return;

    // Cancel any running scan
    mWorker->cancel();
    if (mScanFuture.isRunning())
        mScanFuture.waitForFinished();

    QDir dir(mCurrentPath);
    if (dir.cdUp()) {
        // Don't go above mount point
        QString parentPath = dir.absolutePath();
        if (!parentPath.startsWith(mMountPoint) || parentPath.length() < mMountPoint.length()) {
            parentPath = mMountPoint;
        }
        drillDown(parentPath);
    }
}

void DiskAnalyzerWidget::onScanFinished(const QString &path,
                                         const QList<DirEntry> &entries,
                                         quint64 totalSize)
{
    // Ignore results from a stale scan (user navigated away)
    if (path != mCurrentPath)
        return;

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
    // Only show the last path component for brevity
    QString shortName = currentDir.section('/', -1);
    if (shortName.isEmpty())
        shortName = currentDir;

    mStatusLabel->setText(tr("Scanning... %1 items (%2)")
                          .arg(itemsScanned)
                          .arg(shortName));
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

        mPieSeries->append(e.name, e.size);
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
                    .arg(FormatUtil::formatBytes(static_cast<quint64>(slice->value())))
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
        DiskTreeItem *item = new DiskTreeItem(mTree);

        // Column 0: Name (with folder/file icon)
        item->setText(0, e.name);
        if (e.isDir) {
            item->setIcon(0, QIcon::fromTheme("folder",
                QIcon(":/static/themes/common/img/c_cache.svg")));
        } else {
            item->setIcon(0, QIcon::fromTheme("text-x-generic",
                QIcon(":/static/themes/common/img/c_logs.svg")));
        }

        // Column 1: Size (sortable via UserRole data)
        item->setText(1, FormatUtil::formatBytes(e.size));
        item->setData(1, Qt::UserRole, e.size);

        // Column 2: Percentage (rendered by PercentBarDelegate)
        double pct = parentTotal > 0 ? (double)e.size / (double)parentTotal * 100.0 : 0.0;
        item->setText(2, QString::asprintf("%.1f%%", pct));
        item->setData(2, Qt::UserRole, pct);

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

    // Cancel any running scan
    mWorker->cancel();
    if (mScanFuture.isRunning())
        mScanFuture.waitForFinished();

    QString path = item->data(0, Qt::UserRole).toString();
    drillDown(path);
}

// ─── Breadcrumb ────────────────────────────────────────────────────

void DiskAnalyzerWidget::updateBreadcrumb()
{
    // Show path relative to mount point
    QString rel = mCurrentPath;
    if (mCurrentPath.startsWith(mMountPoint) && mMountPoint.length() > 1) {
        rel = mCurrentPath.mid(mMountPoint.length());
    }
    if (rel.isEmpty()) rel = "/";
    mBreadcrumb->setText(rel);
}

// ─── Theme ─────────────────────────────────────────────────────────

void DiskAnalyzerWidget::onThemeChanged()
{
    auto *styles = AppManager::ins()->getStyleValues();
    if (!styles) return;

    QString chartGridColor = styles->value("@chartGridColor").toString();
    QString bgColor = styles->value("@historyChartBackgroundColor").toString();

    mPieChart->setBackgroundBrush(QColor(bgColor));
    mPieChart->setTitleBrush(QColor(chartGridColor));

    for (int i = 0; i < mPieSeries->count(); ++i) {
        mPieSeries->slices().at(i)->setLabelBrush(QColor(chartGridColor));
    }
}
