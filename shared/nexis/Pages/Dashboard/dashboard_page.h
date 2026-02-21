#ifndef DASHBOARDPAGE_H
#define DASHBOARDPAGE_H

#include <QWidget>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDesktopServices>
#include <QtConcurrent>

#include "Managers/info_manager.h"
#include "metric_tile.h"
#include "network_tile.h"
#include "hero_card.h"
#include "disk_tile.h"

#include "Managers/setting_manager.h"

class AppManager;
class SignalMapper;
class DataRefreshService;

namespace Ui {
    class DashboardPage;
}

class DashboardPage : public QWidget
{
    Q_OBJECT

public:
    explicit DashboardPage(QWidget *parent = nullptr,
                           InfoManager *infoManager = nullptr,
                           SettingManager *settingManager = nullptr,
                           AppManager *appManager = nullptr,
                           SignalMapper *signalMapper = nullptr,
                           DataRefreshService *refreshService = nullptr);
    ~DashboardPage();

protected:
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void onKioskModeChanged(bool enabled);
    void init();
    void checkUpdate();

    void onCpuUpdated(const QList<int> &percents, double clockGHz, const QList<double> &loadAvgs);
    void onMemoryUpdated(quint64 used, quint64 total, quint64 swapUsed, quint64 swapTotal);
    void onNetworkUpdated(quint64 rxBytes, quint64 txBytes);
    void onDiskUsageUpdated(const QList<Disk> &disks);
    void updateTempTile();
    void onGpuUpdated(const QList<GpuDevice> &gpus);
    void onTempSensorChanged(int index);
    void onGpuDeviceChanged(int index);
    void onBatteryUpdated(const BatteryData &bat);
    void onDiskHealthUpdated(const QList<DriveHealth> &drives);

    void on_btnDownloadUpdate_clicked();

signals:
    void sigShowUpdateBar();

private:
    Ui::DashboardPage *ui;

    MetricTile *mCpuTile;
    MetricTile *mMemTile;
    DiskTile *mDiskTile;
    MetricTile *mTempTile;
    MetricTile *mGpuTile;
    MetricTile *mBatteryTile;
    MetricTile *mDiskHealthTile;
    NetworkTile *mNetworkTile;
    HeroCard *mHeroCard;

    QComboBox *mCmbTempSensor;
    QComboBox *mCmbGpuDevice;

    InfoManager *im;
    SettingManager *mSettingManager;
    AppManager *mAppManager;
    SignalMapper *mSignalMapper;
    DataRefreshService *mRefresh;

    int mSelectedSensorIndex;
    int mSelectedGpuIndex;

    QPushButton *mKioskButton;

    QList<QLabel*> mSummaryLabels;

    void buildSystemSummary();
    void buildQuickActions();
};

#endif // DASHBOARDPAGE_H
