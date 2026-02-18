#include "host_manage.h"
#include "ui_host_manage.h"
#include "nexis_roles.h"
#include <qdebug.h>
#include <QRegularExpression>

HostManage::~HostManage()
{
    delete ui;
}

void HostManage::loadIfNeeded()
{
    if (mLoaded)
        return;
    mLoaded = true;
    mHostFileContent = FileUtil::readListFromFile("/etc/hosts");
    loadTableData();
}

HostManage::HostManage(QWidget *parent):
    QWidget(parent),
    mItemModel(new QStandardItemModel(this)),
    mSortFilterModel(new QSortFilterProxyModel(this)),
    updatedLine(-1),
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
    ui->tableViewHosts->horizontalHeader()->setFixedHeight(32);
    ui->tableViewHosts->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    ui->tableViewHosts->horizontalHeader()->setCursor(Qt::PointingHandCursor);
    ui->tableViewHosts->horizontalHeader()->resizeSection(0, 195);
    ui->tableViewHosts->horizontalHeader()->resizeSection(1, 195);

    ui->tableViewHosts->setContextMenuPolicy(Qt::CustomContextMenu);

    loadTableRowMenu();
}

void HostManage::loadHostItems()
{
    mHostItemList.clear();

    int i = 0;
    for (const QString &line: mHostFileContent)
    {
        if (! line.trimmed().startsWith("#") && ! line.trimmed().isEmpty())
        {
            static const QRegularExpression whitespace("\\s+");
            QStringList lineItems = line.trimmed().split(whitespace);

            if (lineItems.count() > 1) {
                HostItem hItem;
                hItem.ip = lineItems.at(0).trimmed();
                hItem.fullQualified = lineItems.at(1).trimmed();
                hItem.aliases = lineItems.count() > 2 ? lineItems.mid(2).join(" ") : "";

                mHostItemList.insert(i, hItem);
            }
        }
        i++;
    }
}

void HostManage::loadTableData()
{
    loadHostItems();

    // Suppress per-row signals during bulk insertion (BUG-06)
    mSortFilterModel->setDynamicSortFilter(false);
    mItemModel->blockSignals(true);

    mItemModel->removeRows(0, mItemModel->rowCount());

    QMapIterator<int,HostItem> itemIterator(mHostItemList);

    while (itemIterator.hasNext()) {
        itemIterator.next();
        mItemModel->appendRow(createRow(QPair<int, HostItem>(itemIterator.key(), itemIterator.value())));
    }

    mItemModel->blockSignals(false);
    mSortFilterModel->setDynamicSortFilter(true);
    mSortFilterModel->invalidate();
    ui->tableViewHosts->reset();

    ui->lblHostTitle->setText(tr("Hosts (%1)").arg(mHostItemList.count()));
}

QList<QStandardItem*> HostManage::createRow(const QPair<int, HostItem> &item)
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
    if (ui->txtIP->text().isEmpty() || ui->txtFullyQualified->text().isEmpty()) {
        ui->lblErrorMsg->setText(tr("The IP and Fully Qualified fields are required."));
        ui->lblErrorMsg->show();
    }
    else {
        QString ip = ui->txtIP->text().trimmed();
        QString fq = ui->txtFullyQualified->text().trimmed();
        QString aliases = ui->txtAliases->text();
        QString line = QString("%1 %2 %3").arg(ip, fq, aliases);

        HostItem hItem;
        hItem.ip = ip;
        hItem.fullQualified = fq;
        hItem.aliases = aliases;

        if (updatedLine == -1) {
            // Add new entry
            int lineNum = mHostFileContent.size();
            mHostFileContent.append(line);
            mHostItemList.insert(lineNum, hItem);
            mItemModel->appendRow(createRow(QPair<int, HostItem>(lineNum, hItem)));
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
    FileUtil::writeFile("/tmp/nexis_etc_host_new_content", mHostFileContent.join("\n"));
    try {
        CommandUtil::sudoExec("mv", {"/tmp/nexis_etc_host_new_content", "/etc/hosts"});
    } catch (QString ex) {
        qDebug() << ex;
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
                // Collect rows to delete (source model indices)
                QList<int> sourceRows;
                while (! selectionModel->selectedRows().isEmpty()) {
                    QModelIndex proxyIndex = selectionModel->selectedRows().first();
                    int lineNumber = mSortFilterModel->index(proxyIndex.row(), 0).data(LineNumberRole).toInt();
                    QModelIndex sourceIndex = mSortFilterModel->mapToSource(proxyIndex);
                    sourceRows.append(sourceIndex.row());

                    mHostFileContent.replace(lineNumber, "");
                    mHostItemList.remove(lineNumber);

                    selectionModel->select(proxyIndex, QItemSelectionModel::Deselect);
                }
                selectionModel->clearSelection();

                // Remove from model in reverse order to preserve indices
                std::sort(sourceRows.begin(), sourceRows.end(), std::greater<int>());
                for (int row : sourceRows)
                    mItemModel->removeRow(row);

                ui->lblHostTitle->setText(tr("Hosts (%1)").arg(mHostItemList.count()));
            }
        }
    }
}
