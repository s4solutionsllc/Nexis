#include "disk_tools_page.h"
#include "ui_disk_tools_page.h"

#include <QButtonGroup>
#include <QComboBox>
#include <QDir>
#include <QDirIterator>
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
#include <QResizeEvent>
#include <QSet>
#include <QVBoxLayout>
#include <QtConcurrent>

#include "Managers/app_manager.h"
#include "Services/duplicate_finder_service.h"
#include "signal_mapper.h"
#include "utilities.h"
#include <Utils/format_util.h>

DiskToolsPage::DiskToolsPage(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::DiskToolsPage)
    , mAppManager(AppManager::ins())
    , mSignalMapper(SignalMapper::ins())
    , mDupService(DuplicateFinderService::ins())
{
    ui->setupUi(this);
    init();
}

DiskToolsPage::~DiskToolsPage()
{
    mLargeOldCancelled.storeRelaxed(1);
    mDupService->cancel();
    disconnect(mDupService, nullptr, this, nullptr);
    mLargeOldFuture.waitForFinished();
    delete ui;
}

void DiskToolsPage::init()
{
    mModeGroup = new QButtonGroup(this);
    mModeGroup->setExclusive(true);
    mModeGroup->addButton(ui->btnModeLargeOld, 0);
    mModeGroup->addButton(ui->btnModeDuplicates, 1);
    connect(mModeGroup, &QButtonGroup::idClicked, this, &DiskToolsPage::switchMode);

    ui->btnModeLargeOld->setCursor(Qt::PointingHandCursor);
    ui->btnModeDuplicates->setCursor(Qt::PointingHandCursor);
    ui->btnModeLargeOld->setObjectName("segmentedLeft");
    ui->btnModeDuplicates->setObjectName("segmentedRight");

    buildLargeOldPage();
    buildDuplicatePage();

    connect(mDupService, &DuplicateFinderService::progressUpdated,
            this, &DiskToolsPage::onDupProgress);
    connect(mDupService, &DuplicateFinderService::scanFinished,
            this, &DiskToolsPage::onDupScanFinished);
    connect(mDupService, &DuplicateFinderService::scanCancelled,
            this, &DiskToolsPage::onDupCancelled);

    connect(this, &DiskToolsPage::largeOldScanFinishedS,
            this, &DiskToolsPage::onLargeOldScanFinished);

    connect(mSignalMapper, &SignalMapper::sigChangedAppTheme,
            this, &DiskToolsPage::refreshThemeColors);

    refreshThemeColors();
}

void DiskToolsPage::switchMode(int index)
{
    ui->stackedModes->setCurrentIndex(index);
}

static void populateDirList(QListWidget *list)
{
    list->clear();
    list->addItem(QStandardPaths::writableLocation(QStandardPaths::HomeLocation));
    QString downloads = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    QString home = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    if (!downloads.isEmpty() && downloads != home)
        list->addItem(downloads);
    QString documents = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    if (!documents.isEmpty() && documents != home)
        list->addItem(documents);
}

void DiskToolsPage::addDirectory()
{
    QString dir = QFileDialog::getExistingDirectory(this, tr("Select Directory"),
        QStandardPaths::writableLocation(QStandardPaths::HomeLocation));
    if (dir.isEmpty())
        return;

    auto addIfMissing = [](QListWidget *list, const QString &d) {
        for (int i = 0; i < list->count(); ++i)
            if (list->item(i)->text() == d) return;
        list->addItem(d);
    };
    addIfMissing(mDirListLargeOld, dir);
    addIfMissing(mDirListDup, dir);
}

void DiskToolsPage::removeDirectory()
{
    QListWidget *current = (ui->stackedModes->currentIndex() == 0) ? mDirListLargeOld : mDirListDup;
    QListWidget *other = (current == mDirListLargeOld) ? mDirListDup : mDirListLargeOld;

    auto *item = current->currentItem();
    if (!item) return;

    QString path = item->text();
    delete item;

    for (int i = 0; i < other->count(); ++i) {
        if (other->item(i)->text() == path) {
            delete other->takeItem(i);
            break;
        }
    }
}

// ---- Large & Old Files Mode ----

void DiskToolsPage::buildLargeOldPage()
{
    auto *layout = new QVBoxLayout(ui->pageLargeOld);
    layout->setContentsMargins(0, 8, 0, 0);
    layout->setSpacing(8);

    // Directory picker
    auto *dirFrame = new QFrame(this);
    auto *dirLayout = new QHBoxLayout(dirFrame);
    dirLayout->setContentsMargins(0, 0, 0, 0);
    dirLayout->setSpacing(8);

    mDirListLargeOld = new QListWidget(dirFrame);
    mDirListLargeOld->setObjectName("diskToolsDirList");
    mDirListLargeOld->setMaximumHeight(80);
    populateDirList(mDirListLargeOld);
    dirLayout->addWidget(mDirListLargeOld, 1);

    auto *dirBtnLayout = new QVBoxLayout();
    dirBtnLayout->setSpacing(4);
    auto *btnAdd = new QPushButton(tr("Add..."), dirFrame);
    btnAdd->setCursor(Qt::PointingHandCursor);
    connect(btnAdd, &QPushButton::clicked, this, &DiskToolsPage::addDirectory);
    dirBtnLayout->addWidget(btnAdd);
    auto *btnRemove = new QPushButton(tr("Remove"), dirFrame);
    btnRemove->setCursor(Qt::PointingHandCursor);
    connect(btnRemove, &QPushButton::clicked, this, &DiskToolsPage::removeDirectory);
    dirBtnLayout->addWidget(btnRemove);
    dirBtnLayout->addStretch();
    dirLayout->addLayout(dirBtnLayout);
    layout->addWidget(dirFrame);

    // Filter container — layout rebuilt dynamically in applyLargeOldFilterLayout()
    mLargeOldFilterWidget = new QWidget(ui->pageLargeOld);

    mLblSize = new QLabel(tr("Size >="), mLargeOldFilterWidget);
    mSpinSize = new QSpinBox(mLargeOldFilterWidget);
    mSpinSize->setRange(1, 99999);
    mSpinSize->setValue(100);

    mCbSizeUnit = new QComboBox(mLargeOldFilterWidget);
    mCbSizeUnit->addItems({"MB", "GB"});

    mLblNotAccessed = new QLabel(tr("Not accessed in >="), mLargeOldFilterWidget);
    mSpinAge = new QSpinBox(mLargeOldFilterWidget);
    mSpinAge->setRange(1, 99999);
    mSpinAge->setValue(180);

    mCbAgeUnit = new QComboBox(mLargeOldFilterWidget);
    mCbAgeUnit->addItems({tr("days"), tr("months"), tr("years")});

    mLblMatch = new QLabel(tr("Match:"), mLargeOldFilterWidget);
    mCbFilterMode = new QComboBox(mLargeOldFilterWidget);
    mCbFilterMode->addItems({tr("Either"), tr("Large only"), tr("Old only")});

    mBtnLargeOldCancel = new QPushButton(tr("Cancel"), mLargeOldFilterWidget);
    mBtnLargeOldCancel->setCursor(Qt::PointingHandCursor);
    mBtnLargeOldCancel->hide();
    connect(mBtnLargeOldCancel, &QPushButton::clicked, this, [this]() {
        mLargeOldCancelled.storeRelaxed(1);
    });

    mBtnLargeOldScan = new QPushButton(tr("Scan"), mLargeOldFilterWidget);
    mBtnLargeOldScan->setObjectName("btnScan");
    mBtnLargeOldScan->setCursor(Qt::PointingHandCursor);
    connect(mBtnLargeOldScan, &QPushButton::clicked, this, &DiskToolsPage::onLargeOldScan);

    applyLargeOldFilterLayout(false);
    layout->addWidget(mLargeOldFilterWidget);

    // Status
    mLblLargeOldStatus = new QLabel(this);
    mLblLargeOldStatus->setObjectName("lblStatus");
    layout->addWidget(mLblLargeOldStatus);

    // Results tree
    mTreeLargeOld = new QTreeWidget(this);
    mTreeLargeOld->setObjectName("treeWidgetLargeOld");
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
    connect(mTreeLargeOld, &QTreeWidget::itemChanged, this, &DiskToolsPage::updateLargeOldSelection);
    layout->addWidget(mTreeLargeOld, 1);

    // Action bar
    auto *actionBar = new QFrame(this);
    actionBar->setObjectName("actionBarFrame");
    auto *actionLayout = new QHBoxLayout(actionBar);
    actionLayout->setContentsMargins(8, 8, 8, 8);
    actionLayout->setSpacing(12);

    mLblLargeOldSelection = new QLabel(tr("No files selected"), actionBar);
    actionLayout->addWidget(mLblLargeOldSelection);
    actionLayout->addStretch();

    mBtnLargeOldTrash = new QPushButton(tr("Move to Trash"), actionBar);
    mBtnLargeOldTrash->setObjectName("btnTrash");
    mBtnLargeOldTrash->setCursor(Qt::PointingHandCursor);
    mBtnLargeOldTrash->setEnabled(false);
    connect(mBtnLargeOldTrash, &QPushButton::clicked, this, &DiskToolsPage::onLargeOldTrash);
    actionLayout->addWidget(mBtnLargeOldTrash);

    layout->addWidget(actionBar);
}

void DiskToolsPage::onLargeOldScan()
{
    if (mLargeOldFuture.isRunning() || mDirListLargeOld->count() == 0)
        return;

    mLargeOldCancelled.storeRelaxed(0);
    mBtnLargeOldScan->hide();
    mBtnLargeOldCancel->show();
    mTreeLargeOld->clear();
    mLblLargeOldStatus->setText(tr("Scanning..."));

    qint64 sizeThreshold = mSpinSize->value();
    if (mCbSizeUnit->currentIndex() == 0)
        sizeThreshold *= 1024LL * 1024;
    else
        sizeThreshold *= 1024LL * 1024 * 1024;

    int ageValue = mSpinAge->value();
    int ageUnitIdx = mCbAgeUnit->currentIndex();
    qint64 ageMinutes;
    if (ageUnitIdx == 0) ageMinutes = static_cast<qint64>(ageValue) * 24 * 60;
    else if (ageUnitIdx == 1) ageMinutes = static_cast<qint64>(ageValue) * 30 * 24 * 60;
    else ageMinutes = static_cast<qint64>(ageValue) * 365 * 24 * 60;

    int filterMode = mCbFilterMode->currentIndex();

    QStringList dirs;
    for (int i = 0; i < mDirListLargeOld->count(); ++i)
        dirs.append(mDirListLargeOld->item(i)->text());

    mLargeOldFuture = QtConcurrent::run([this, dirs, sizeThreshold, ageMinutes, filterMode]() {
        QList<QFileInfo> results;

        for (const QString &dir : dirs) {
            if (mLargeOldCancelled.loadRelaxed())
                break;

            QDirIterator it(dir, QDir::Files | QDir::NoDotAndDotDot,
                            QDirIterator::Subdirectories);
            while (it.hasNext()) {
                if (mLargeOldCancelled.loadRelaxed())
                    break;

                it.next();
                QFileInfo info = it.fileInfo();

                if (info.isSymLink())
                    continue;

                bool isLarge = info.size() >= sizeThreshold;

                QDateTime accessTime = info.lastRead();
                qint64 minutesAgo = accessTime.secsTo(QDateTime::currentDateTime()) / 60;
                bool isOld = minutesAgo >= ageMinutes;

                bool matches = false;
                if (filterMode == 0) matches = isLarge || isOld;
                else if (filterMode == 1) matches = isLarge;
                else matches = isOld;

                if (matches)
                    results.append(info);
            }
        }

        if (mLargeOldCancelled.loadRelaxed())
            return;

        std::sort(results.begin(), results.end(), [](const QFileInfo &a, const QFileInfo &b) {
            return a.size() > b.size();
        });

        emit largeOldScanFinishedS(results);
    });
}

void DiskToolsPage::onLargeOldScanFinished(const QList<QFileInfo> &results)
{
    mBtnLargeOldCancel->hide();
    mBtnLargeOldScan->show();
    mLargeOldResults = results;

    mTreeLargeOld->setUpdatesEnabled(false);

    for (const QFileInfo &fi : mLargeOldResults) {
        auto *item = new QTreeWidgetItem(mTreeLargeOld);
        item->setCheckState(0, Qt::Unchecked);
        item->setText(0, fi.fileName());
        item->setText(1, fi.absolutePath());
        item->setText(2, FormatUtil::formatBytes(fi.size()));
        item->setData(2, Qt::UserRole, static_cast<qulonglong>(fi.size()));
        item->setText(3, fi.lastRead().toString("yyyy-MM-dd hh:mm"));
        item->setText(4, fi.lastModified().toString("yyyy-MM-dd hh:mm"));
        item->setData(0, Qt::UserRole, fi.absoluteFilePath());
    }

    mTreeLargeOld->setUpdatesEnabled(true);
    mLblLargeOldStatus->setText(tr("%1 files found").arg(mLargeOldResults.size()));
    updateLargeOldSelection();
}

void DiskToolsPage::onLargeOldTrash()
{
    QStringList filesToTrash;
    quint64 totalSize = 0;
    for (int i = 0; i < mTreeLargeOld->topLevelItemCount(); ++i) {
        auto *item = mTreeLargeOld->topLevelItem(i);
        if (item->checkState(0) == Qt::Checked) {
            filesToTrash.append(item->data(0, Qt::UserRole).toString());
            totalSize += item->data(2, Qt::UserRole).toULongLong();
        }
    }

    if (filesToTrash.isEmpty())
        return;

    auto reply = QMessageBox::question(this, tr("Move to Trash"),
        tr("Move %1 files (%2) to trash?").arg(filesToTrash.size()).arg(FormatUtil::formatBytes(totalSize)),
        QMessageBox::Yes | QMessageBox::No);

    if (reply != QMessageBox::Yes)
        return;

    int trashed = 0;
    for (const QString &path : filesToTrash) {
        if (QFile::moveToTrash(path))
            trashed++;
    }

    for (int i = mTreeLargeOld->topLevelItemCount() - 1; i >= 0; --i) {
        auto *item = mTreeLargeOld->topLevelItem(i);
        if (item->checkState(0) == Qt::Checked) {
            QString path = item->data(0, Qt::UserRole).toString();
            if (!QFile::exists(path))
                delete mTreeLargeOld->takeTopLevelItem(i);
        }
    }

    mLblLargeOldStatus->setText(tr("Moved %1 files to trash").arg(trashed));
    updateLargeOldSelection();
}

void DiskToolsPage::updateLargeOldSelection()
{
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
        mLblLargeOldSelection->setText(tr("%1 files selected (%2)")
            .arg(count).arg(FormatUtil::formatBytes(size)));
        mBtnLargeOldTrash->setEnabled(true);
    } else {
        mLblLargeOldSelection->setText(tr("No files selected"));
        mBtnLargeOldTrash->setEnabled(false);
    }
}

// ---- Duplicate Finder Mode ----

void DiskToolsPage::buildDuplicatePage()
{
    auto *layout = new QVBoxLayout(ui->pageDuplicates);
    layout->setContentsMargins(0, 8, 0, 0);
    layout->setSpacing(8);

    // Directory picker (separate widget, synced data)
    auto *dirFrame = new QFrame(this);
    auto *dirLayout = new QHBoxLayout(dirFrame);
    dirLayout->setContentsMargins(0, 0, 0, 0);
    dirLayout->setSpacing(8);

    mDirListDup = new QListWidget(dirFrame);
    mDirListDup->setObjectName("diskToolsDirList");
    mDirListDup->setMaximumHeight(80);
    populateDirList(mDirListDup);
    dirLayout->addWidget(mDirListDup, 1);

    auto *dirBtnLayout = new QVBoxLayout();
    dirBtnLayout->setSpacing(4);
    auto *btnAdd = new QPushButton(tr("Add..."), dirFrame);
    btnAdd->setCursor(Qt::PointingHandCursor);
    connect(btnAdd, &QPushButton::clicked, this, &DiskToolsPage::addDirectory);
    dirBtnLayout->addWidget(btnAdd);
    auto *btnRemove = new QPushButton(tr("Remove"), dirFrame);
    btnRemove->setCursor(Qt::PointingHandCursor);
    connect(btnRemove, &QPushButton::clicked, this, &DiskToolsPage::removeDirectory);
    dirBtnLayout->addWidget(btnRemove);
    dirBtnLayout->addStretch();
    dirLayout->addLayout(dirBtnLayout);
    layout->addWidget(dirFrame);

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
    mCbMinDupUnit->setCurrentIndex(1);
    filterLayout->addWidget(mCbMinDupUnit);

    filterLayout->addSpacing(16);

    filterLayout->addWidget(new QLabel(tr("File pattern:"), this));
    mEditGlob = new QLineEdit(this);
    mEditGlob->setPlaceholderText(tr("e.g., *.jpg or leave empty for all"));
    mEditGlob->setMaximumWidth(200);
    filterLayout->addWidget(mEditGlob);

    filterLayout->addStretch();

    mBtnDupCancel = new QPushButton(tr("Cancel"), this);
    mBtnDupCancel->setCursor(Qt::PointingHandCursor);
    mBtnDupCancel->hide();
    connect(mBtnDupCancel, &QPushButton::clicked, this, [this]() {
        mDupService->cancel();
    });
    filterLayout->addWidget(mBtnDupCancel);

    mBtnDupScan = new QPushButton(tr("Find Duplicates"), this);
    mBtnDupScan->setObjectName("btnScan");
    mBtnDupScan->setCursor(Qt::PointingHandCursor);
    connect(mBtnDupScan, &QPushButton::clicked, this, &DiskToolsPage::onDupScan);
    filterLayout->addWidget(mBtnDupScan);

    layout->addLayout(filterLayout);

    // Progress
    mDupProgress = new QProgressBar(this);
    mDupProgress->setTextVisible(false);
    mDupProgress->hide();
    layout->addWidget(mDupProgress);

    mLblDupStatus = new QLabel(this);
    mLblDupStatus->setObjectName("lblStatus");
    layout->addWidget(mLblDupStatus);

    // Results tree (grouped)
    mTreeDuplicates = new QTreeWidget(this);
    mTreeDuplicates->setObjectName("treeWidgetDuplicates");
    mTreeDuplicates->setHeaderLabels({tr("Name / Group"), tr("Path"), tr("Size"), tr("Last Modified")});
    mTreeDuplicates->setRootIsDecorated(true);
    mTreeDuplicates->setSortingEnabled(false);
    mTreeDuplicates->setAlternatingRowColors(true);
    mTreeDuplicates->header()->setStretchLastSection(true);
    mTreeDuplicates->header()->setSectionResizeMode(0, QHeaderView::Interactive);
    mTreeDuplicates->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    mTreeDuplicates->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    mTreeDuplicates->header()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    connect(mTreeDuplicates, &QTreeWidget::itemChanged, this, &DiskToolsPage::updateDupSelection);
    layout->addWidget(mTreeDuplicates, 1);

    // Action bar
    auto *actionBar = new QFrame(this);
    actionBar->setObjectName("actionBarFrame");
    auto *actionLayout = new QHBoxLayout(actionBar);
    actionLayout->setContentsMargins(8, 8, 8, 8);
    actionLayout->setSpacing(12);

    mLblDupSelection = new QLabel(tr("No files selected"), actionBar);
    actionLayout->addWidget(mLblDupSelection);
    actionLayout->addStretch();

    mBtnDupTrash = new QPushButton(tr("Move to Trash"), actionBar);
    mBtnDupTrash->setObjectName("btnTrash");
    mBtnDupTrash->setCursor(Qt::PointingHandCursor);
    mBtnDupTrash->setEnabled(false);
    connect(mBtnDupTrash, &QPushButton::clicked, this, &DiskToolsPage::onDupTrash);
    actionLayout->addWidget(mBtnDupTrash);

    layout->addWidget(actionBar);
}

void DiskToolsPage::onDupScan()
{
    if (mDupService->isScanning() || mDirListDup->count() == 0)
        return;

    mTreeDuplicates->clear();
    mDupProgress->show();
    mDupProgress->setRange(0, 0);
    mBtnDupScan->hide();
    mBtnDupCancel->show();

    qint64 minSize = mSpinMinDupSize->value();
    int unitIdx = mCbMinDupUnit->currentIndex();
    if (unitIdx == 0) minSize *= 1024LL;
    else if (unitIdx == 1) minSize *= 1024LL * 1024;
    else minSize *= 1024LL * 1024 * 1024;

    QString glob = mEditGlob->text().trimmed();

    QStringList dirs;
    for (int i = 0; i < mDirListDup->count(); ++i)
        dirs.append(mDirListDup->item(i)->text());

    mLblDupStatus->setText(tr("Starting scan..."));
    mDupService->scan(dirs, minSize, glob);
}

void DiskToolsPage::onDupProgress(int stage, int current, int total, const QString &message)
{
    Q_UNUSED(stage);
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

    // FW-08: retain for the trash path so the service-side last-copy guard
    // sees the full group, not just the rows the user happens to have checked.
    mDupResults = results;

    mTreeDuplicates->setUpdatesEnabled(false);

    quint64 totalWasted = 0;

    for (const DuplicateGroup &group : results) {
        quint64 wastedBytes = group.fileSize * static_cast<quint64>(group.files.size() - 1);
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
            child->setData(2, Qt::UserRole, static_cast<qulonglong>(fi.size()));
            child->setText(3, fi.lastModified().toString("yyyy-MM-dd hh:mm"));
            child->setData(0, Qt::UserRole, fi.absoluteFilePath());
        }
    }

    mTreeDuplicates->expandAll();
    mTreeDuplicates->setUpdatesEnabled(true);

    mLblDupStatus->setText(tr("%1 duplicate groups found — %2 wasted space")
        .arg(results.size()).arg(FormatUtil::formatBytes(totalWasted)));

    updateDupSelection();
}

void DiskToolsPage::onDupCancelled()
{
    mDupProgress->hide();
    mBtnDupCancel->hide();
    mBtnDupScan->show();
    mLblDupStatus->setText(tr("Scan cancelled"));
}

void DiskToolsPage::onDupTrash()
{
    QStringList filesToTrash;
    quint64 totalSize = 0;

    for (int g = 0; g < mTreeDuplicates->topLevelItemCount(); ++g) {
        auto *groupItem = mTreeDuplicates->topLevelItem(g);
        for (int c = 0; c < groupItem->childCount(); ++c) {
            auto *child = groupItem->child(c);
            if (child->checkState(0) == Qt::Checked) {
                filesToTrash.append(child->data(0, Qt::UserRole).toString());
                totalSize += child->data(2, Qt::UserRole).toULongLong();
            }
        }
    }

    if (filesToTrash.isEmpty())
        return;

    auto reply = QMessageBox::question(this, tr("Move to Trash"),
        tr("Move %1 duplicate files (%2) to trash?")
            .arg(filesToTrash.size()).arg(FormatUtil::formatBytes(totalSize)),
        QMessageBox::Yes | QMessageBox::No);

    if (reply != QMessageBox::Yes)
        return;

    // FW-08 (SSO-3736): hand the selection to the service so the cleaner
    // exclusion engine and the never-delete-last-copy invariant are
    // enforced server-side; the UI's checkbox bookkeeping alone can't
    // guarantee them once the user starts re-checking the "kept" row.
    const QStringList trashedPaths =
        mDupService->trashFiles(filesToTrash, mDupResults);
    const int trashed = trashedPaths.size();
    const QSet<QString> trashedSet(trashedPaths.constBegin(), trashedPaths.constEnd());

    for (int g = mTreeDuplicates->topLevelItemCount() - 1; g >= 0; --g) {
        auto *groupItem = mTreeDuplicates->topLevelItem(g);
        for (int c = groupItem->childCount() - 1; c >= 0; --c) {
            auto *child = groupItem->child(c);
            const QString path = child->data(0, Qt::UserRole).toString();
            if (trashedSet.contains(path))
                delete groupItem->takeChild(c);
        }
        if (groupItem->childCount() <= 1)
            delete mTreeDuplicates->takeTopLevelItem(g);
    }

    const int skipped = filesToTrash.size() - trashed;
    if (skipped > 0) {
        mLblDupStatus->setText(
            tr("Moved %1 files to trash · %2 kept to honor exclusions or "
               "preserve at least one copy")
                .arg(trashed).arg(skipped));
    } else {
        mLblDupStatus->setText(tr("Moved %1 files to trash").arg(trashed));
    }
    updateDupSelection();
}

void DiskToolsPage::updateDupSelection()
{
    int count = 0;
    quint64 size = 0;
    for (int g = 0; g < mTreeDuplicates->topLevelItemCount(); ++g) {
        auto *groupItem = mTreeDuplicates->topLevelItem(g);
        for (int c = 0; c < groupItem->childCount(); ++c) {
            auto *child = groupItem->child(c);
            if (child->checkState(0) == Qt::Checked) {
                count++;
                size += child->data(2, Qt::UserRole).toULongLong();
            }
        }
    }

    if (count > 0) {
        mLblDupSelection->setText(tr("%1 files selected (%2)")
            .arg(count).arg(FormatUtil::formatBytes(size)));
        mBtnDupTrash->setEnabled(true);
    } else {
        mLblDupSelection->setText(tr("No files selected"));
        mBtnDupTrash->setEnabled(false);
    }
}

void DiskToolsPage::refreshThemeColors()
{
}

void DiskToolsPage::applyLargeOldFilterLayout(bool compact)
{
    delete mLargeOldFilterWidget->layout();
    mLargeOldFilterCompact = compact;

    if (!compact) {
        auto *row = new QHBoxLayout(mLargeOldFilterWidget);
        row->setSpacing(8);
        row->addWidget(mLblSize);
        row->addWidget(mSpinSize);
        row->addWidget(mCbSizeUnit);
        row->addSpacing(16);
        row->addWidget(mLblNotAccessed);
        row->addWidget(mSpinAge);
        row->addWidget(mCbAgeUnit);
        row->addSpacing(16);
        row->addWidget(mLblMatch);
        row->addWidget(mCbFilterMode);
        row->addStretch();
        row->addWidget(mBtnLargeOldCancel);
        row->addWidget(mBtnLargeOldScan);
    } else {
        // Three rows — one group per row to avoid crowding
        auto *col = new QVBoxLayout(mLargeOldFilterWidget);
        col->setSpacing(8);
        col->setContentsMargins(0, 0, 0, 0);

        auto *row1 = new QHBoxLayout();
        row1->setSpacing(8);
        row1->addWidget(mLblSize);
        row1->addWidget(mSpinSize);
        row1->addWidget(mCbSizeUnit);
        row1->addStretch();
        col->addLayout(row1);

        auto *row2 = new QHBoxLayout();
        row2->setSpacing(8);
        row2->addWidget(mLblNotAccessed);
        row2->addWidget(mSpinAge);
        row2->addWidget(mCbAgeUnit);
        row2->addStretch();
        col->addLayout(row2);

        auto *row3 = new QHBoxLayout();
        row3->setSpacing(8);
        row3->addWidget(mLblMatch);
        row3->addWidget(mCbFilterMode);
        row3->addStretch();
        row3->addWidget(mBtnLargeOldCancel);
        row3->addWidget(mBtnLargeOldScan);
        col->addLayout(row3);
    }
}

void DiskToolsPage::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    if (!mLargeOldFilterWidget)
        return;
    const bool compact = event->size().width() < 720;
    if (compact != mLargeOldFilterCompact)
        applyLargeOldFilterLayout(compact);
}
