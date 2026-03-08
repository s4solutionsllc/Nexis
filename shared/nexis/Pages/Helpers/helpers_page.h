#ifndef HELPERS_PAGE_H
#define HELPERS_PAGE_H

#include <QWidget>
#include "host_manage.h"
#include "utilities.h"

class QPushButton;
class QLabel;
class NetworkDiagWidget;
class OpenPortsWidget;

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
    void on_btnNetDiag_clicked();
    void on_btnOpenPorts_clicked();
    void onRebuildSpotlight();
    void onVerifyDisk();
    void onRebuildLaunchServices();
    void onPowerProfileClicked();
    void init();

private:
    Ui::HelpersPage *ui;

    HostManage *widgetHostManage;
    NetworkDiagWidget *mNetworkDiagWidget;
    OpenPortsWidget *mOpenPortsWidget;

#ifdef Q_OS_MACOS
    QPushButton *mBtnRebuildSpotlight = nullptr;
    QPushButton *mBtnVerifyDisk = nullptr;
    QPushButton *mBtnRebuildLaunchServices = nullptr;
#else
    QWidget *mPowerProfileWidget = nullptr;
    QPushButton *mBtnPowerSaver = nullptr;
    QPushButton *mBtnBalanced = nullptr;
    QPushButton *mBtnPerformance = nullptr;
    QLabel *mLblConflictWarning = nullptr;

    void initPowerProfileUI();
    void updatePowerProfileButtons();
    void applyPowerProfileStyle();
#endif
};

#endif // HELPERS_PAGE_H
