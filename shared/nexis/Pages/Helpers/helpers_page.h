#ifndef HELPERS_PAGE_H
#define HELPERS_PAGE_H

#include <QWidget>
#include "host_manage.h"
#include "utilities.h"

class QPushButton;

namespace Ui {
class HelpersPage;
}

class HelpersPage : public QWidget
{
    Q_OBJECT

public:
    explicit HelpersPage(QWidget *parent = 0);
    ~HelpersPage();

private slots:
    void on_btnHostManage_clicked();
    void on_btnFlushDNS_clicked();
    void onRebuildSpotlight();
    void onVerifyDisk();
    void onRebuildLaunchServices();
    void init();

private:
    Ui::HelpersPage *ui;

    HostManage *widgetHostManage;

#ifdef Q_OS_MACOS
    QPushButton *mBtnRebuildSpotlight = nullptr;
    QPushButton *mBtnVerifyDisk = nullptr;
    QPushButton *mBtnRebuildLaunchServices = nullptr;
#endif
};

#endif // HELPERS_PAGE_H
