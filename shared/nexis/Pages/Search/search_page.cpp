#include "search_page.h"
#include "ui_search_page.h"
#include "nexis_roles.h"
#include "dpi.h"
#include "Services/file_search_service.h"
#include <qdebug.h>
#include <QClipboard>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QResizeEvent>
#include <QVBoxLayout>

SearchPage::SearchPage(QWidget *parent, InfoManager *infoManager,
                       SettingManager *settingManager, FileSearchService *searchService) :
    QWidget(parent),
    ui(new Ui::SearchPage),
    mInfoManager(infoManager ? infoManager : InfoManager::ins()),
    mSettingManager(settingManager ? settingManager : SettingManager::ins()),
    mSearchService(searchService ? searchService : FileSearchService::ins()),
    mItemModel(new QStandardItemModel(this)),
    mSortFilterModel(new QSortFilterProxyModel(this))
{
    ui->setupUi(this);

    init();
}

SearchPage::~SearchPage()
{
    delete ui;
}

void SearchPage::init()
{
    mTableHeaders = QStringList {
        tr("Name"), tr("Path"), tr("Size"), tr("User"), tr("Group"),
        tr("Creation Time"), tr("Last Access"), tr("Last Modification"), tr("Last Change"),
    };

    // Table settings
    mItemModel->setHorizontalHeaderLabels(mTableHeaders);
    mSortFilterModel->setSourceModel(mItemModel);

    ui->tableFoundResults->setModel(mSortFilterModel);
    mSortFilterModel->setSortRole(SortRole);
    mSortFilterModel->setDynamicSortFilter(true);
    mSortFilterModel->sort(1, Qt::SortOrder::DescendingOrder);

    ui->tableFoundResults->horizontalHeader()->setSectionsMovable(true);
    ui->tableFoundResults->horizontalHeader()->setFixedHeight(Dpi::scale(32));
    ui->tableFoundResults->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    ui->tableFoundResults->horizontalHeader()->setCursor(Qt::PointingHandCursor);
    ui->tableFoundResults->horizontalHeader()->resizeSection(0, 150);
    ui->tableFoundResults->horizontalHeader()->resizeSection(1, 150);

    ui->tableFoundResults->horizontalHeader()->setContextMenuPolicy(Qt::CustomContextMenu);
    ui->tableFoundResults->setContextMenuPolicy(Qt::CustomContextMenu);

    connect(ui->tableFoundResults->horizontalHeader(), &QHeaderView::customContextMenuRequested,
        this, &SearchPage::tableFoundResults_header_customContextMenuRequested);

    loadHeaderMenu();
    loadTableRowMenu();

    rowRole = SortRole;
    mSearchResultDateFormat = "dd.MM.yyyy hh:mm:ss";

    applyAdvancedSearchLayout(false);

    ui->advanceSearchPane->setHidden(false);
    on_btnAdvancePaneToggle_clicked();

    ui->lblErrorMsg->hide();

    QString iconLoading = QString(":/static/themes/%1/img/loading.gif").arg(mSettingManager->getThemeName());
    QMovie *loadingMovie = new QMovie(iconLoading, QByteArray(), this);
    ui->lblLoadingSearching->setMovie(loadingMovie);
    loadingMovie->start();
    ui->lblLoadingSearching->hide();

    connect(mSearchService, &FileSearchService::searchFinished,
            this, &SearchPage::onSearchFinished);

    connect(mSearchService, &FileSearchService::fileOperationFinished,
            this, &SearchPage::onFileOperationFinished);

    initComboboxValues();

    QList<QWidget*> widgets = {
        ui->btnBrowseSearchDir, ui->btnSearchAdvance, ui->txtSearchInput, ui->cmbGroups,
        ui->cmbSizeCriteria, ui->cmbSizeUnits, ui->cmbTimeCriteria, ui->cmbTimeType,
        ui->cmbSearchTypes, ui->tableFoundResults, ui->cmbUsers
    };

    Utilities::addDropShadow(widgets, 30);
}

void SearchPage::loadTableRowMenu()
{
    QAction *actionOpenFolder = new QAction(QIcon(":/static/themes/common/img/folder.png"), tr("Open Folder"));
    actionOpenFolder->setData("open-folder");
    mTableRowMenu.addAction(actionOpenFolder);

    QAction *actionMoveTrash = new QAction(QIcon(":/static/themes/common/img/trash_2.png"), tr("Move Trash"));
    actionMoveTrash->setData("move-trash");
    mTableRowMenu.addAction(actionMoveTrash);

    QAction *actionDelete = new QAction(QIcon(":/static/themes/common/img/delete.png"), tr("Delete"));
    actionDelete->setData("delete");
    mTableRowMenu.addAction(actionDelete);
}

void SearchPage::loadHeaderMenu()
{
    int i = 0;
    QList<QAction*> actionList;
    actionList.reserve(mTableHeaders.size());

    for (const QString &header : mTableHeaders) {
        QAction *action = new QAction(header,&mHeaderMenu);
        action->setCheckable(true);
        action->setChecked(true);
        action->setData(i++);
        actionList.push_back(action);
    }
    mHeaderMenu.addActions(actionList);
    // exclude headers
    QList<int> hiddenHeaders = { 4, 6, 7, 8 };

    QList<QAction*> actions = mHeaderMenu.actions();
    for (const int i : hiddenHeaders) {
        if (i < mTableHeaders.count()) {
            ui->tableFoundResults->horizontalHeader()->setSectionHidden(i, true);
            actions.at(i)->setChecked(false);
        }
    }
}

void SearchPage::initComboboxValues()
{
    ui->cmbUsers->addItem(tr("Choose"), "-1");
    ui->cmbUsers->addItems(mInfoManager->getUserList());

    ui->cmbGroups->addItem(tr("Choose"), "-1");
    ui->cmbGroups->addItems(mInfoManager->getGroupList());

    ui->cmbSearchTypes->addItem(tr("All"), "all");
    ui->cmbSearchTypes->addItem(tr("File"), "f");
    ui->cmbSearchTypes->addItem(tr("Directory"), "d");
    ui->cmbSearchTypes->addItem(tr("Symbolic Link"), "l");

    ui->cmbTimeType->addItem(tr("Choose"), "-1");
    ui->cmbTimeType->addItem(tr("Access"), "-amin");
    ui->cmbTimeType->addItem(tr("Modify"), "-mmin");
    ui->cmbTimeType->addItem(tr("Change"), "-cmin");

    ui->cmbTimeCriteria->addItem(tr("Smaller (<)"), "-");
    ui->cmbTimeCriteria->addItem(tr("Equal (=)"), "");
    ui->cmbTimeCriteria->addItem(tr("Greater (>)"), "+");

    ui->cmbSizeCriteria->addItem(tr("Choose"), "-1");
    ui->cmbSizeCriteria->addItem(tr("Smaller (<)"), "-");
    ui->cmbSizeCriteria->addItem(tr("Equal (=)"), "");
    ui->cmbSizeCriteria->addItem(tr("Greater (>)"), "+");

    ui->cmbSizeUnits->addItem(tr("Bytes"), "c");
    ui->cmbSizeUnits->addItem(tr("Kibibytes"), "k");
    ui->cmbSizeUnits->addItem(tr("Mebibytes"), "M");
    ui->cmbSizeUnits->addItem(tr("Gibibytes"), "G");
}

void SearchPage::on_btnBrowseSearchDir_clicked()
{
    QString selectedDirPath = QFileDialog::getExistingDirectory(this, tr("Select Directory"), "/",
                                      QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);

    QDir selectedDir(selectedDirPath);

    if (! selectedDirPath.isEmpty() && selectedDir.exists()) {
        ui->lblSearchDir->setText(tr("Directory: %1").arg(selectedDirPath));
        mSelectedDirectory = selectedDirPath;
    }
}

void SearchPage::on_btnAdvancePaneToggle_clicked()
{
    ui->advanceSearchPane->setHidden(! ui->advanceSearchPane->isHidden());
    QString icon = ui->advanceSearchPane->isHidden() ? "▼" : "▲";
    ui->btnAdvancePaneToggle->setText(tr("Advanced Search %1").arg(icon));
}

void SearchPage::on_btnSearchAdvance_clicked()
{
    if (mSelectedDirectory.isEmpty()) {
        ui->lblErrorMsg->show();
        ui->lblErrorMsg->setText(tr("Select the search directory."));
        return;
    }

    ui->lblErrorMsg->hide();
    ui->advanceSearchPane->hide();
    ui->lblLoadingSearching->show();
    ui->btnSearchAdvance->setEnabled(false);

    // Build search params from UI widgets
    FileSearchParams params;
    params.directory = mSelectedDirectory;
    params.namePattern = ui->txtSearchInput->text();
    params.caseInsensitive = ui->checkCaseInsensitive->isChecked();
    params.isRegex = ui->checkRegEx->isChecked();
    params.invertMatch = ui->checkInvert->isChecked();
    params.findEmpty = ui->checkEmpty->isChecked();
    params.fileType = ui->cmbSearchTypes->currentData().toString();
    params.timeType = ui->cmbTimeType->currentData().toString();
    params.timeCriteria = ui->cmbTimeCriteria->currentData().toString();
    params.timeValue = ui->spinTime->value();
    params.permReadable = ui->checkPermReadable->isChecked();
    params.permWritable = ui->checkPermWritable->isChecked();
    params.permExecutable = ui->checkPermExecutable->isChecked();
    params.sizeCriteria = ui->cmbSizeCriteria->currentData().toString();
    params.sizeValue = ui->spinSize->value();
    params.sizeUnit = ui->cmbSizeUnits->currentData().toString();
    params.searchAsRoot = ui->checkSearchAsRoot->isChecked();

    if (ui->cmbUsers->currentData().toString() != "-1")
        params.userName = ui->cmbUsers->currentText();
    if (ui->cmbGroups->currentData().toString() != "-1")
        params.groupName = ui->cmbGroups->currentText();

    mSearchService->search(params);
}

void SearchPage::onSearchFinished(const QStringList &results, bool hadError)
{
    if (hadError) {
        ui->lblErrorMsg->show();
        ui->lblErrorMsg->setText(tr("Somethings went wrong, try again."));
    } else if (results.isEmpty()) {
        mItemModel->removeRows(0, mItemModel->rowCount());
    } else {
        loadDataToTable(results);
    }

    ui->lblLoadingSearching->hide();
    ui->btnSearchAdvance->setEnabled(true);
}

void SearchPage::onFileOperationFinished(FileSearchService::FileOperation op,
                                         QString filePath,
                                         bool hadError,
                                         QString errorMessage)
{
    Q_UNUSED(op)

    if (hadError) {
        ui->lblErrorMsg->show();
        ui->lblErrorMsg->setText(errorMessage.isEmpty()
                                 ? tr("File operation failed.")
                                 : tr("File operation failed: %1").arg(errorMessage));
        return;
    }

    // Find and remove the row whose path matches; the file is gone from disk.
    for (int row = mSortFilterModel->rowCount() - 1; row >= 0; --row) {
        const QString fileName = mSortFilterModel->index(row, 0).data(rowRole).toString();
        const QString folderPath = mSortFilterModel->index(row, 1).data(rowRole).toString();
        if (folderPath + "/" + fileName == filePath) {
            mSortFilterModel->removeRow(row);
            break;
        }
    }
}

void SearchPage::loadDataToTable(const QList<QString> &foundFiles)
{
    mItemModel->removeRows(0, mItemModel->rowCount());

    for (const QString &file : foundFiles.mid(1, 2000)) {
        mItemModel->appendRow(createRow(file));
    }

    ui->lblFoundFilesInfo->setText(tr("%1 files found. Showing %2 of them.")
                                   .arg(foundFiles.count()-1)
                                   .arg(mItemModel->rowCount()));
}

QList<QStandardItem*> SearchPage::createRow(const QString &filepath)
{
    QFileInfo *fileInfo = new QFileInfo(filepath);

    QStandardItem *i_name = new QStandardItem(fileInfo->fileName());
    i_name->setData(fileInfo->fileName(), rowRole);
    i_name->setData(fileInfo->fileName(), Qt::ToolTipRole);

    QStandardItem *i_path = new QStandardItem(fileInfo->path());
    i_path->setData(fileInfo->path(), rowRole);
    i_path->setData(fileInfo->path(), Qt::ToolTipRole);

    QStandardItem *i_size = new QStandardItem(FormatUtil::formatBytes(fileInfo->size()));
    i_size->setData(fileInfo->size(), rowRole);
    i_size->setData(fileInfo->size(), Qt::ToolTipRole);

    QStandardItem *i_user = new QStandardItem(fileInfo->owner());
    i_user->setData(fileInfo->owner(), rowRole);
    i_user->setData(fileInfo->owner(), Qt::ToolTipRole);

    QStandardItem *i_group = new QStandardItem(fileInfo->group());
    i_group->setData(fileInfo->group(), rowRole);
    i_group->setData(fileInfo->group(), Qt::ToolTipRole);

    QStandardItem *i_creationTime = new QStandardItem(fileInfo->birthTime().toString(mSearchResultDateFormat));
    i_creationTime->setData(fileInfo->birthTime().toString(mSearchResultDateFormat), rowRole);
    i_creationTime->setData(fileInfo->birthTime().toString(mSearchResultDateFormat), Qt::ToolTipRole);

    QStandardItem *i_lastAccess = new QStandardItem(fileInfo->lastRead().toString(mSearchResultDateFormat));
    i_lastAccess->setData(fileInfo->lastRead().toString(mSearchResultDateFormat), rowRole);
    i_lastAccess->setData(fileInfo->lastRead().toString(mSearchResultDateFormat), Qt::ToolTipRole);

    QStandardItem *i_lastModify = new QStandardItem(fileInfo->lastModified().toString(mSearchResultDateFormat));
    i_lastModify->setData(fileInfo->lastModified().toString(mSearchResultDateFormat), rowRole);
    i_lastModify->setData(fileInfo->lastModified().toString(mSearchResultDateFormat), Qt::ToolTipRole);

    QStandardItem *i_lastChange = new QStandardItem(fileInfo->metadataChangeTime().toString(mSearchResultDateFormat));
    i_lastChange->setData(fileInfo->metadataChangeTime().toString(mSearchResultDateFormat), rowRole);
    i_lastChange->setData(fileInfo->metadataChangeTime().toString(mSearchResultDateFormat), Qt::ToolTipRole);

    delete fileInfo;

    return {
        i_name, i_path, i_size, i_user, i_group,
        i_creationTime, i_lastAccess, i_lastModify, i_lastChange
    };
}

void SearchPage::tableFoundResults_header_customContextMenuRequested(const QPoint &pos)
{
    QPoint globalPos = ui->tableFoundResults->mapToGlobal(pos);
    QAction *action = mHeaderMenu.exec(globalPos);

    if (action) {
        ui->tableFoundResults->horizontalHeader()->setSectionHidden(action->data().toInt(), ! action->isChecked());
    }
}

void SearchPage::on_tableFoundResults_customContextMenuRequested(const QPoint &pos)
{
    if (mItemModel->rowCount() > 0) {
        QPoint globalPos = ui->tableFoundResults->mapToGlobal(pos);
        QAction *action = mTableRowMenu.exec(globalPos);

        QModelIndexList selecteds = ui->tableFoundResults->selectionModel()->selectedRows();
        QItemSelectionModel *selectionModel = ui->tableFoundResults->selectionModel();

        if (action && ! selecteds.isEmpty()) {
            if (action->data().toString() == "open-folder") {
                for (QModelIndex &index : selecteds) {
                    QUrl folderPath = mSortFilterModel->index(index.row(), 1).data(rowRole).toUrl();
                    QDesktopServices::openUrl(folderPath);
                }
            }
            else if (action->data().toString() == "move-trash"
                     || action->data().toString() == "delete") {
                // SSO-3365: FileSearchService now dispatches on a worker thread
                // and emits fileOperationFinished. We collect the paths up front
                // and react to the signal — see onFileOperationFinished — so a
                // long delete doesn't freeze the UI or crash the slot via a
                // thrown QString from CommandUtil::exec.
                const bool moveToTrash = action->data().toString() == "move-trash";
                const QString currentUser = mInfoManager->getUserName();

                QStringList pendingPaths;
                for (const QModelIndex &index : selecteds) {
                    QString fileName = mSortFilterModel->index(index.row(), 0).data(rowRole).toString();
                    QString folderPath = mSortFilterModel->index(index.row(), 1).data(rowRole).toString();
                    pendingPaths << (folderPath + "/" + fileName);
                }

                selectionModel->clearSelection();
                ui->lblErrorMsg->hide();

                for (const QString &filePath : pendingPaths) {
                    const QString fileName = QFileInfo(filePath).fileName();
                    if (moveToTrash) {
                        mSearchService->moveToTrash(filePath, fileName, currentUser);
                    } else {
                        mSearchService->deleteFile(filePath, currentUser);
                    }
                }
            }
        }
    }
}

void SearchPage::on_tableFoundResults_doubleClicked(const QModelIndex &index)
{
    QUrl folderPath = mSortFilterModel->index(index.row(), 1).data(rowRole).toUrl();
    QDesktopServices::openUrl(folderPath);
}

void SearchPage::applyAdvancedSearchLayout(bool compact)
{
    delete ui->advanceSearchPane->layout();
    mAdvancedSearchCompact = compact;

    if (!compact) {
        // Two-column QGridLayout matching the original .ui structure
        auto *grid = new QGridLayout(ui->advanceSearchPane);
        grid->setSpacing(12);
        grid->setContentsMargins(0, 5, 0, 2);

        // Row 0 — checkboxes spanning all 8 columns
        grid->addWidget(ui->checkSearchAsRoot,   0, 0);
        grid->addWidget(ui->checkRegEx,          0, 1);
        grid->addWidget(ui->checkCaseInsensitive,0, 2);
        grid->setColumnStretch(3, 1);
        grid->addWidget(ui->checkInvert,         0, 4);
        grid->addWidget(ui->lblFileOrFolder,     0, 5);
        grid->addWidget(ui->checkEmpty,          0, 6);
        grid->setColumnStretch(7, 1);

        // Row 1 — section labels
        grid->addWidget(ui->lblTime,        1, 0, 1, 4);
        grid->addWidget(ui->lblPermissions, 1, 4, 1, 3);

        // Row 2 — Time controls (left) | Permission checkboxes (right)
        auto *timeRow = new QHBoxLayout();
        timeRow->setSpacing(10);
        timeRow->addWidget(ui->cmbTimeType);
        timeRow->addWidget(ui->cmbTimeCriteria);
        timeRow->addWidget(ui->spinTime);
        grid->addLayout(timeRow, 2, 0, 1, 4);

        auto *permRow = new QHBoxLayout();
        permRow->setSpacing(5);
        permRow->addWidget(ui->checkPermReadable);
        permRow->addWidget(ui->checkPermWritable);
        permRow->addWidget(ui->checkPermExecutable);
        grid->addLayout(permRow, 2, 4, 1, 3);

        // Row 3 — Size label (left) | Owner label (right)
        grid->addWidget(ui->lblSize,  3, 0, 1, 4);
        grid->addWidget(ui->lblOwner, 3, 4, 1, 3);

        // Row 4 — Size controls (left) | Owner combos (right)
        auto *sizeRow = new QHBoxLayout();
        sizeRow->setSpacing(10);
        sizeRow->addWidget(ui->cmbSizeCriteria);
        sizeRow->addWidget(ui->spinSize);
        sizeRow->addWidget(ui->cmbSizeUnits);
        grid->addLayout(sizeRow, 4, 0, 1, 4);

        auto *ownerRow = new QHBoxLayout();
        ownerRow->setSpacing(10);
        ownerRow->addWidget(ui->cmbUsers);
        ownerRow->addWidget(ui->cmbGroups);
        grid->addLayout(ownerRow, 4, 4, 1, 3);
    } else {
        // Single-column VBoxLayout — all groups stacked vertically
        auto *col = new QVBoxLayout(ui->advanceSearchPane);
        col->setSpacing(8);
        col->setContentsMargins(0, 5, 0, 2);

        // Checkbox rows — split into two HBox rows
        auto *cbRow1 = new QHBoxLayout();
        cbRow1->setSpacing(12);
        cbRow1->addWidget(ui->checkSearchAsRoot);
        cbRow1->addWidget(ui->checkRegEx);
        cbRow1->addWidget(ui->checkCaseInsensitive);
        cbRow1->addStretch();
        col->addLayout(cbRow1);

        auto *cbRow2 = new QHBoxLayout();
        cbRow2->setSpacing(12);
        cbRow2->addWidget(ui->checkInvert);
        cbRow2->addWidget(ui->lblFileOrFolder);
        cbRow2->addWidget(ui->checkEmpty);
        cbRow2->addStretch();
        col->addLayout(cbRow2);

        // Time group
        col->addWidget(ui->lblTime);
        auto *timeRow = new QHBoxLayout();
        timeRow->setSpacing(10);
        timeRow->addWidget(ui->cmbTimeType);
        timeRow->addWidget(ui->cmbTimeCriteria);
        timeRow->addWidget(ui->spinTime);
        col->addLayout(timeRow);

        // Permissions group
        col->addWidget(ui->lblPermissions);
        auto *permRow = new QHBoxLayout();
        permRow->setSpacing(12);
        permRow->addWidget(ui->checkPermReadable);
        permRow->addWidget(ui->checkPermWritable);
        permRow->addWidget(ui->checkPermExecutable);
        permRow->addStretch();
        col->addLayout(permRow);

        // Size group
        col->addWidget(ui->lblSize);
        auto *sizeRow = new QHBoxLayout();
        sizeRow->setSpacing(10);
        sizeRow->addWidget(ui->cmbSizeCriteria);
        sizeRow->addWidget(ui->spinSize);
        sizeRow->addWidget(ui->cmbSizeUnits);
        col->addLayout(sizeRow);

        // Owner group
        col->addWidget(ui->lblOwner);
        auto *ownerRow = new QHBoxLayout();
        ownerRow->setSpacing(10);
        ownerRow->addWidget(ui->cmbUsers);
        ownerRow->addWidget(ui->cmbGroups);
        ownerRow->addStretch();
        col->addLayout(ownerRow);
    }
}

void SearchPage::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    if (ui->advanceSearchPane->isHidden())
        return;
    const bool compact = event->size().width() < 560;
    if (compact != mAdvancedSearchCompact)
        applyAdvancedSearchLayout(compact);
}
