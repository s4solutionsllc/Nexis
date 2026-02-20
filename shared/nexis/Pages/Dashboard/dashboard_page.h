#ifndef DASHBOARDPAGE_H
#define DASHBOARDPAGE_H

#include <QWidget>
#include <QTimer>
#include <QComboBox>
#include <QPushButton>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDesktopServices>
#include <QtConcurrent>

#include "Managers/info_manager.h"
#include "circlebar.h"
#include "linebar.h"

#include "Managers/setting_manager.h"

class AppManager;
class SignalMapper;

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
                           SignalMapper *signalMapper = nullptr);
    ~DashboardPage();

protected:
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void onKioskModeChanged(bool enabled);
    void init();
    void checkUpdate();

    void updateCpuBar();
    void updateMemoryBar();
    void updateDiskBar();
    void updateNetworkBar();
    void updateTempBar();
    void onTempSensorChanged(int index);

    void updateGpuBar();
    void onGpuDeviceChanged(int index);

    void updateBatteryBar();
    void updateDiskHealthBar();

    void on_btnDownloadUpdate_clicked();

signals:
    void sigShowUpdateBar();

private:
    Ui::DashboardPage *ui;

private:
    CircleBar* mCpuBar;
    CircleBar* mMemBar;
    CircleBar* mDiskBar;
    CircleBar* mTempBar;
    CircleBar* mGpuBar;
    CircleBar* mBatteryBar;
    CircleBar* mDiskHealthBar;

    LineBar *mDownloadBar;
    LineBar *mUploadBar;

    QTimer *mTimer;
    InfoManager *im;
    SettingManager *mSettingManager;
    AppManager *mAppManager;
    SignalMapper *mSignalMapper;

    int mSelectedSensorIndex;
    int mSelectedGpuIndex;

    QPushButton *mKioskButton;
};

#endif // DASHBOARDPAGE_H
