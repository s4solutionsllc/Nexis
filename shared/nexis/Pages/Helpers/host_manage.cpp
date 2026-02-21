#include "host_manage.h"
#include "ui_host_manage.h"
#include "nexis_roles.h"
#include "dpi.h"
#include <qdebug.h>
#include <QMessageBox>

HostManage::~HostManage()
{
    delete ui;
}

void HostManage::loadIfNeeded()
{
    if (mLoaded)
        return;
    mLoaded = true;
    mHostFileContent = mHostService->readHostFile();
    mOriginalHostFileContent = mHostFileContent;
    loadTableData();
}

HostManage::HostManage(QWidget *parent, HostService *hostService):
    QWidget(parent),
    mItemModel(new QStandardItemModel(this)),
    mSortFilterModel(new QSortFilterProxyModel(this)),
    updatedLine(-1),
    mHostService(hostService ? hostService : HostService::ins()),
    ui(new Ui::HostManage)
{
    ui->setupUi(this);

    init();
}

void HostManage::init()
{
    ui->lblHostTitle->setText(tr("Hosts (%1)").arg(1));

    Utilities::addDropShadow({
        ui->btnCancel, ui->btnNewHost, ui->btnSave, ui->txtAliases, ui->txtFullyQualified,
        ui->txtIP, ui->tableViewHosts
    }, 40);

    ui->widgetAddEditHost->hide();
    ui->lblErrorMsg->hide();

    mHeaderList = {
       tr("IP Address"), tr("Full Qualified"), tr("Aliases")
    };

    mItemModel->setHorizontalHeaderLabels(mHeaderList);
    mSortFilterModel->setSourceModel(mItemModel);

    ui->tableViewHosts->setModel(mSortFilterModel);
    mSortFilterModel->setSortRole(SortRole);
    mSortFilterModel->setDynamicSortFilter(true);

    ui->tableViewHosts->horizontalHeader()->setSectionsMovable(true);
    ui->tableViewHosts->horizontalHeader()->setFixedHeight(Dpi::scale(32));
    ui->tableViewHosts->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    ui->tableViewHosts->horizontalHeader()->setCursor(Qt::PointingHandCursor);
    ui->tableViewHosts->horizontalHeader()->resizeSection(0, 195);
    ui->tableViewHosts->horizontalHeader()->resizeSection(1, 195);

    ui->tableViewHosts->setContextMenuPolicy(Qt::CustomContextMenu);

    loadTableRowMenu();
}

void HostManage::loadHostItems()
{
    mHostItemList = mHostService->parseHostEntries(mHostFileContent);
}

void HostManage::loadTableData()
{
    loadHostItems();

    // Suppress per-row signals during bulk insertion (BUG-06)
    mSortFilterModel->setDynamicSortFilter(false);
    mItemModel->blockSignals(true);

    mItemModel->removeRows(0, mItemModel->rowCount());

    QMapIterator<int, HostEntry> itemIterator(mHostItemList);

    while (itemIterator.hasNext()) {
        itemIterator.next();
        mItemModel->appendRow(createRow(QPair<int, HostEntry>(itemIterator.key(), itemIterator.value())));
    }

    mItemModel->blockSignals(false);
    mSortFilterModel->setDynamicSortFilter(true);
    mSortFilterModel->invalidate();
    ui->tableViewHosts->reset();

    ui->lblHostTitle->setText(tr("Hosts (%1)").arg(mHostItemList.count()));
}

QList<QStandardItem*> HostManage::createRow(const QPair<int, HostEntry> &item)
{
    QStandardItem *i_ip = new QStandardItem(item.second.ip);
    i_ip->setData(item.first, LineNumberRole);
    i_ip->setData(item.second.ip, SortRole);
    i_ip->setData(item.second.ip, Qt::ToolTipRole);

    QStandardItem *i_fullQualified = new QStandardItem(item.second.fullQualified);
    i_fullQualified->setData(item.second.fullQualified, SortRole);
    i_fullQualified->setData(item.second.fullQualified, Qt::ToolTipRole);

    QStandardItem *i_aliases = new QStandardItem(item.second.aliases);
    i_aliases->setData(item.second.aliases, SortRole);
    i_aliases->setData(item.second.aliases, Qt::ToolTipRole);

    return {
        i_ip, i_fullQualified, i_aliases
    };
}

void HostManage::on_btnNewHost_clicked()
{
    ui->widgetAddEditHost->show();
    ui->lblErrorMsg->hide();

    ui->txtIP->clear();
    ui->txtFullyQualified->clear();
    ui->txtAliases->clear();

    updatedLine = -1;
}

void HostManage::loadTableRowMenu()
{
    QAction *actionOpenFolder = new QAction(QIcon(":/static/themes/common/img/folder.png"), tr("Edit"),&mTableRowMenu);
    actionOpenFolder->setData("edit");
    mTableRowMenu.addAction(actionOpenFolder);

    QAction *actionDelete = new QAction(QIcon(":/static/themes/common/img/delete.png"), tr("Delete"),&mTableRowMenu);
    actionDelete->setData("delete");
    mTableRowMenu.addAction(actionDelete);
}

void HostManage::on_btnSave_clicked()
{
    QString ip = ui->txtIP->text().trimmed();
    QString fq = ui->txtFullyQualified->text().trimmed();
    QString aliases = ui->txtAliases->text().trimmed();

    if (ip.isEmpty() || fq.isEmpty()) {
        ui->lblErrorMsg->setText(tr("The IP and Fully Qualified fields are required."));
        ui->lblErrorMsg->show();
        return;
    }

    if (!HostService::isValidIP(ip)) {
        ui->lblErrorMsg->setText(tr("Invalid IP address format."));
        ui->lblErrorMsg->show();
        return;
    }

    if (!HostService::isValidHostname(fq)) {
        ui->lblErrorMsg->setText(tr("Invalid hostname format."));
        ui->lblErrorMsg->show();
        return;
    }

    if (!aliases.isEmpty()) {
        static const QRegularExpression whitespace("\\s+");
        QStringList aliasList = aliases.split(whitespace, Qt::SkipEmptyParts);
        for (const QString &alias : aliasList) {
            if (!HostService::isValidHostname(alias)) {
                ui->lblErrorMsg->setText(tr("Invalid alias format: %1").arg(alias));
                ui->lblErrorMsg->show();
                return;
            }
        }
    }

    ui->lblErrorMsg->hide();

    {
        QString line = aliases.isEmpty() ? QString("%1 %2").arg(ip, fq) : QString("%1 %2 %3").arg(ip, fq, aliases);

        HostEntry hItem;
        hItem.ip = ip;
        hItem.fullQualified = fq;
        hItem.aliases = aliases;

        if (updatedLine == -1) {
            // Add new entry
            int lineNum = mHostFileContent.size();
            mHostFileContent.append(line);
            mHostItemList.insert(lineNum, hItem);
            mItemModel->appendRow(createRow(QPair<int, HostEntry>(lineNum, hItem)));
        } else {
            // Edit existing entry
            mHostFileContent.replace(updatedLine, line);
            mHostItemList[updatedLine] = hItem;

            // Find and update the model row with matching LineNumberRole
            for (int r = 0; r < mItemModel->rowCount(); ++r) {
                if (mItemModel->item(r, 0)->data(LineNumberRole).toInt() == updatedLine) {
                    mItemModel->item(r, 0)->setText(ip);
                    mItemModel->item(r, 0)->setData(ip, SortRole);
                    mItemModel->item(r, 0)->setData(ip, Qt::ToolTipRole);
                    mItemModel->item(r, 1)->setText(fq);
                    mItemModel->item(r, 1)->setData(fq, SortRole);
                    mItemModel->item(r, 1)->setData(fq, Qt::ToolTipRole);
                    mItemModel->item(r, 2)->setText(aliases);
                    mItemModel->item(r, 2)->setData(aliases, SortRole);
                    mItemModel->item(r, 2)->setData(aliases, Qt::ToolTipRole);
                    break;
                }
            }
        }

        updatedLine = -1;
        ui->lblHostTitle->setText(tr("Hosts (%1)").arg(mHostItemList.count()));
        ui->widgetAddEditHost->hide();
    }
}

void HostManage::on_btnCancel_clicked()
{
    ui->widgetAddEditHost->hide();
    ui->lblErrorMsg->hide();

    updatedLine = -1;
}

void HostManage::on_btnSaveChanges_clicked()
{
    if (mHostFileContent == mOriginalHostFileContent) {
        ui->lblChangesMsg->setText(tr("No changes to save."));
        return;
    }

    // Count changes for the confirmation summary
    int added = 0, deleted = 0, modified = 0;
    int maxLines = qMax(mHostFileContent.size(), mOriginalHostFileContent.size());
    for (int i = 0; i < maxLines; ++i) {
        if (i >= mOriginalHostFileContent.size())
            ++added;
        else if (i >= mHostFileContent.size())
            ++deleted;
        else if (mHostFileContent.at(i) != mOriginalHostFileContent.at(i))
            ++modified;
    }

    QStringList parts;
    if (added > 0)
        parts << tr("%n entry(s) added", "", added);
    if (modified > 0)
        parts << tr("%n entry(s) modified", "", modified);
    if (deleted > 0)
        parts << tr("%n entry(s) deleted", "", deleted);

    QString message = tr("Save changes to /etc/hosts?\n\n%1.\n\n"
                         "A backup will be created at /etc/hosts.nexis-backup.")
                      .arg(parts.join(", "));

    QMessageBox::StandardButton reply = QMessageBox::question(
        this, tr("Confirm Save"), message,
        QMessageBox::Save | QMessageBox::Cancel,
        QMessageBox::Cancel);

    if (reply != QMessageBox::Save)
        return;

    // Create backup (best-effort — warn but don't block on failure)
    if (!mHostService->createBackup()) {
        QMessageBox::StandardButton proceed = QMessageBox::warning(
            this, tr("Backup Failed"),
            tr("Could not create backup of /etc/hosts.\n\n"
               "Do you want to save anyway?"),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No);
        if (proceed != QMessageBox::Yes)
            return;
    }

    // Write via the host service
    if (mHostService->saveHostFile(mHostFileContent)) {
        mOriginalHostFileContent = mHostFileContent;
        ui->lblChangesMsg->setText(tr("Changes saved successfully."));
    } else {
        QMessageBox::critical(
            this, tr("Save Failed"),
            tr("Failed to write to /etc/hosts."));
    }
}

void HostManage::on_tableViewHosts_customContextMenuRequested(const QPoint &pos)
{
    if (mItemModel->rowCount() > 0) {
        QPoint globalPos = ui->tableViewHosts->mapToGlobal(pos);
        QAction *action = mTableRowMenu.exec(globalPos);

        QModelIndexList selecteds = ui->tableViewHosts->selectionModel()->selectedRows();
        QItemSelectionModel *selectionModel = ui->tableViewHosts->selectionModel();

        if (action && ! selecteds.isEmpty()) {
            if (action->data().toString() == "edit") {
                QModelIndex index = selectionModel->selectedRows().first();

                updatedLine = mSortFilterModel->index(index.row(), 0).data(LineNumberRole).toInt();

                ui->txtIP->setText(mHostItemList.value(updatedLine).ip);
                ui->txtFullyQualified->setText(mHostItemList.value(updatedLine).fullQualified);
                ui->txtAliases->setText(mHostItemList.value(updatedLine).aliases);

                ui->widgetAddEditHost->show();

                selectionModel->clearSelection();
            }
            else if (action->data().toString() == "delete") {
                // Collect line numbers to delete from backing store
                QList<int> lineNumbers;
                while (! selectionModel->selectedRows().isEmpty()) {
                    QModelIndex proxyIndex = selectionModel->selectedRows().first();
                    int lineNumber = mSortFilterModel->index(proxyIndex.row(), 0).data(LineNumberRole).toInt();
                    lineNumbers.append(lineNumber);
                    selectionModel->select(proxyIndex, QItemSelectionModel::Deselect);
                }
                selectionModel->clearSelection();

                // Remove from backing store in descending order to preserve indices
                std::sort(lineNumbers.begin(), lineNumbers.end(), std::greater<int>());
                for (int lineNum : lineNumbers)
                    mHostFileContent.removeAt(lineNum);

                // Rebuild the parsed map and table model with correct line numbers
                loadTableData();
            }
        }
    }
}
