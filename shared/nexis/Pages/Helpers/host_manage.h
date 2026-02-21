#ifndef HOST_MANAGE_H
#define HOST_MANAGE_H

#include <QWidget>
#include <QStandardItemModel>
#include <QSortFilterProxyModel>
#include <QMenu>

#include "utilities.h"
#include "Services/host_service.h"

namespace Ui {
class HostManage;
}

class HostManage : public QWidget
{
    Q_OBJECT

public:
    explicit HostManage(QWidget *parent = nullptr,
                        HostService *hostService = nullptr);
    ~HostManage();
    void loadIfNeeded();

private slots:
    void init();

    void on_btnNewHost_clicked();
    void on_btnSave_clicked();
    void loadHostItems();
    void loadTableData();
    void on_btnCancel_clicked();
    void loadTableRowMenu();
    void on_btnSaveChanges_clicked();
    QList<QStandardItem *> createRow(const QPair<int, HostEntry> &item);

    void on_tableViewHosts_customContextMenuRequested(const QPoint &pos);

private:
    Ui::HostManage *ui;

    QList<QString> mHeaderList;

    QStandardItemModel *mItemModel;
    QSortFilterProxyModel *mSortFilterModel;
    QMenu mTableRowMenu;

    HostService *mHostService;

    QStringList mHostFileContent;
    QStringList mOriginalHostFileContent;
    QMap<int, HostEntry> mHostItemList;

    int updatedLine;
    bool mLoaded = false;
};

#endif // HOST_MANAGE_H
