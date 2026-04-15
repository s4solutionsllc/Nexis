#ifndef HELPERS_PAGE_H
#define HELPERS_PAGE_H

#include <QList>
#include <QWidget>
#include "host_manage.h"
#include "utilities.h"

class QPushButton;
class QLabel;
class QResizeEvent;
class NetworkDiagWidget;
class OpenPortsWidget;
class FirewallWidget;

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
    void on_btnFirewall_clicked();
    void onRebuildSpotlight();
    void onVerifyDisk();
    void onRebuildLaunchServices();
    void onPowerProfileClicked();
    void init();

    void applyNavLayout(bool compact);
    void computeNavMinWidth();
    void resizeEvent(QResizeEvent *event) override;

private:
    Ui::HelpersPage *ui;

    HostManage *widgetHostManage;
    NetworkDiagWidget *mNetworkDiagWidget;
    OpenPortsWidget *mOpenPortsWidget;
    FirewallWidget *mFirewallWidget;

    QList<QWidget*> mNavItems;
    bool mNavCompact = false;
    int mNavMinWidth = 0;

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
#endif
};

#endif // HELPERS_PAGE_H
