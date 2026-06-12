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
class SwappinessWidget;
class CpuTuningWidget;
class BatteryChargeThresholdWidget;
class TrimWidget;
class WolWidget;

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
    void onSwappinessClicked();             // FR-81
    void onCpuTuningClicked();             // FR-117
    void onBatteryThresholdClicked();      // FW-15
    void onTrimClicked();                  // FR-118
    void onWolClicked();                   // FR-120
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
    TrimWidget *mTrimWidget = nullptr;           // FR-118 — both platforms
    QPushButton *mBtnTrim = nullptr;
#ifdef Q_OS_LINUX
    SwappinessWidget              *mSwappinessWidget          = nullptr;
    QPushButton                   *mBtnSwappiness             = nullptr;
    CpuTuningWidget               *mCpuTuningWidget           = nullptr;
    QPushButton                   *mBtnCpuTuning              = nullptr;
    BatteryChargeThresholdWidget  *mBatteryThresholdWidget    = nullptr;
    QPushButton                   *mBtnBatteryThreshold       = nullptr;
#endif
    WolWidget   *mWolWidget = nullptr;
    QPushButton *mBtnWol    = nullptr;

    QList<QWidget*> mToolItems;
    bool mNavCompact = false;
    int mNavMinWidth = 0;

    QWidget *mToolsContainer = nullptr;
    QWidget *mMaintenanceSection = nullptr;
    void buildMaintenanceSection();

#ifndef Q_OS_MACOS
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
