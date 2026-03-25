#include "exclusion_manager_dialog.h"
#include <Managers/app_manager.h>

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QPushButton>
#include <QToolButton>
#include <QHeaderView>
#include <QFileDialog>
#include <QMessageBox>
#include <QScrollArea>

ExclusionManagerDialog::ExclusionManagerDialog(QWidget *parent, AppManager *appManager)
    : QDialog(parent),
      mAppManager(appManager ? appManager : AppManager::ins())
{
    buildUI();
    refreshList();
}

void ExclusionManagerDialog::buildUI()
{
    setWindowTitle(tr("Exclusion Rules"));
    setMinimumSize(520, 400);
    setObjectName("exclusionManagerDialog");

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(12);
    mainLayout->setContentsMargins(20, 15, 20, 15);

    mLblTitle = new QLabel(tr("Manage Exclusion Rules"));
    mLblTitle->setProperty("accessibleName", "dialog-title");
    mainLayout->addWidget(mLblTitle);

    mLblNotice = new QLabel(tr("These paths will be skipped during scanning and cleaning."));
    mLblNotice->setObjectName("lblExclusionNotice");
    mLblNotice->setWordWrap(true);
    mainLayout->addWidget(mLblNotice);

    mTree = new QTreeWidget;
    mTree->setColumnCount(2);
    mTree->setHeaderLabels({tr("Type"), tr("Path")});
    mTree->header()->setStretchLastSection(true);
    mTree->header()->resizeSection(0, 80);
    mTree->setSelectionMode(QAbstractItemView::SingleSelection);
    mTree->setRootIsDecorated(false);
    mTree->setAlternatingRowColors(true);
    mTree->setFocusPolicy(Qt::NoFocus);
    mainLayout->addWidget(mTree, 1);

    QHBoxLayout *btnRow = new QHBoxLayout;

    mBtnAddFile = new QToolButton;
    mBtnAddFile->setText(tr("Add File..."));
    mBtnAddFile->setToolButtonStyle(Qt::ToolButtonTextOnly);
    mBtnAddFile->setAutoRaise(true);
    mBtnAddFile->setCursor(Qt::PointingHandCursor);

    mBtnAddFolder = new QToolButton;
    mBtnAddFolder->setText(tr("Add Folder..."));
    mBtnAddFolder->setToolButtonStyle(Qt::ToolButtonTextOnly);
    mBtnAddFolder->setAutoRaise(true);
    mBtnAddFolder->setCursor(Qt::PointingHandCursor);

    mBtnRemove = new QToolButton;
    mBtnRemove->setText(tr("Remove"));
    mBtnRemove->setToolButtonStyle(Qt::ToolButtonTextOnly);
    mBtnRemove->setAutoRaise(true);
    mBtnRemove->setCursor(Qt::PointingHandCursor);

    btnRow->addWidget(mBtnAddFile);
    btnRow->addWidget(mBtnAddFolder);
    btnRow->addWidget(mBtnRemove);
    btnRow->addStretch();
    mainLayout->addLayout(btnRow);

    mBtnClose = new QPushButton(tr("Close"));
    mBtnClose->setProperty("accessibleName", "primary");
    QHBoxLayout *closeRow = new QHBoxLayout;
    closeRow->addStretch();
    closeRow->addWidget(mBtnClose);
    mainLayout->addLayout(closeRow);

    connect(mBtnAddFile, &QToolButton::clicked, this, &ExclusionManagerDialog::onAddFile);
    connect(mBtnAddFolder, &QToolButton::clicked, this, &ExclusionManagerDialog::onAddFolder);
    connect(mBtnRemove, &QToolButton::clicked, this, &ExclusionManagerDialog::onRemoveSelected);
    connect(mBtnClose, &QPushButton::clicked, this, &QDialog::accept);
}

void ExclusionManagerDialog::refreshList()
{
    mTree->clear();
    QList<CleanerService::ExclusionEntry> entries = CleanerService::ins()->loadExclusions();
    for (const CleanerService::ExclusionEntry &e : entries) {
        QTreeWidgetItem *item = new QTreeWidgetItem(mTree);
        item->setText(0, e.type == CleanerService::ExclusionEntry::Folder ? tr("Folder") : tr("File"));
        item->setText(1, e.path);
    }
}

void ExclusionManagerDialog::onAddFile()
{
    QString path = QFileDialog::getOpenFileName(this, tr("Select File to Exclude"));
    if (path.isEmpty())
        return;

    QList<CleanerService::ExclusionEntry> entries = CleanerService::ins()->loadExclusions();
    for (const CleanerService::ExclusionEntry &e : entries) {
        if (e.path == path) {
            QMessageBox::information(this, tr("Duplicate"),
                tr("This path is already in the exclusion list."));
            return;
        }
    }

    CleanerService::ins()->addExclusion(CleanerService::ExclusionEntry::File, path);
    refreshList();
}

void ExclusionManagerDialog::onAddFolder()
{
    QString path = QFileDialog::getExistingDirectory(this, tr("Select Folder to Exclude"));
    if (path.isEmpty())
        return;

    QList<CleanerService::ExclusionEntry> entries = CleanerService::ins()->loadExclusions();
    for (const CleanerService::ExclusionEntry &e : entries) {
        if (e.path == path) {
            QMessageBox::information(this, tr("Duplicate"),
                tr("This path is already in the exclusion list."));
            return;
        }
    }

    CleanerService::ins()->addExclusion(CleanerService::ExclusionEntry::Folder, path);
    refreshList();
}

void ExclusionManagerDialog::onRemoveSelected()
{
    QTreeWidgetItem *item = mTree->currentItem();
    if (!item)
        return;

    QString path = item->text(1);
    CleanerService::ins()->removeExclusion(path);
    refreshList();
}
