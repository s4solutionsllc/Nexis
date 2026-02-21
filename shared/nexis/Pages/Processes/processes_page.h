#ifndef PROCESSESPAGE_H
#define PROCESSESPAGE_H

#include <QWidget>
#include <QStandardItemModel>
#include <QSortFilterProxyModel>
#include <QDebug>
#include <QScrollBar>
#include <QMenu>
#include <QAction>

#include "Managers/info_manager.h"

class DataRefreshService;
class ProcessService;

namespace Ui {
    class ProcessesPage;
}

class ProcessesPage : public QWidget
{
    Q_OBJECT

public:
    explicit ProcessesPage(QWidget *parent = nullptr,
                           InfoManager *infoManager = nullptr,
                           DataRefreshService *refreshService = nullptr,
                           ProcessService *processService = nullptr);
    ~ProcessesPage();

private slots:
    void init();
    void onProcessesUpdated(const QList<Process> &processes, const QString &userName);
    void loadHeaderMenu();
    QList<QStandardItem *> createRow(const Process &proc);
    void on_txtProcessSearch_textChanged(const QString &val);
    void on_sliderRefresh_valueChanged(const int &i);
    void on_btnEndProcess_clicked();
    void on_tableProcess_customContextMenuRequested(const QPoint &pos);

private:
    Ui::ProcessesPage *ui;

    QStandardItemModel *mItemModel;
    QSortFilterProxyModel *mSortFilterModel;
    QModelIndex mSelectedRowModel;
    QStringList mHeaders;
    QMenu mHeaderMenu;
    InfoManager *im;
    DataRefreshService *mRefresh;
    ProcessService *mProcessService;
};

#endif // PROCESSESPAGE_H
