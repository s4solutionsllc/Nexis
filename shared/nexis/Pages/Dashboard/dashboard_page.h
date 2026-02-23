#ifndef DASHBOARDPAGE_H
#define DASHBOARDPAGE_H

#include <QWidget>
#include <QComboBox>
#include <QMenu>
#include <QPushButton>
#include <QLabel>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDesktopServices>
#include <QShortcut>
#include <QtConcurrent>

#include "Managers/info_manager.h"
#include "metric_tile.h"
#include "network_tile.h"
#include "disk_tile.h"
#include "dashboard_tile_wrapper.h"

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
    void onTempSensorSelected(QAction *action);
    void onGpuDeviceChanged(int index);
    void onDiskSelected(QAction *action);
    void onBatteryUpdated(const BatteryData &bat);
    void onDiskHealthUpdated(const QList<DriveHealth> &drives);

    void on_btnDownloadUpdate_clicked();
    void toggleEditMode();
    void exitEditMode();
    void onResetLayout();
    void onTileDragStarted(DashboardTileWrapper *wrapper, const QPoint &globalPos);
    void onTileDragMoved(DashboardTileWrapper *wrapper, const QPoint &globalPos);
    void onTileDragFinished(DashboardTileWrapper *wrapper, const QPoint &globalPos);
    void onTileResizeRequested(DashboardTileWrapper *wrapper, int newColSpan, int newRowSpan);

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
    NetworkTile *mNetworkTile;

    QComboBox *mCmbGpuDevice;
    QMenu *mDiskMenu;
    QMenu *mTempSensorMenu;
    QList<Disk> mCachedDisks;
    QList<DriveHealth> mCachedDriveHealth;

    InfoManager *im;
    SettingManager *mSettingManager;
    AppManager *mAppManager;
    SignalMapper *mSignalMapper;
    DataRefreshService *mRefresh;

    int mSelectedSensorIndex;
    int mSelectedGpuIndex;

    QPushButton *mKioskButton;
    QPushButton *mEditButton;
    QWidget *mEditToolbar;
    QPushButton *mBtnResetLayout;
    QPushButton *mBtnDone;
    QShortcut *mEditShortcut;
    bool mEditMode;
    bool mKioskMode;

    QList<DashboardTileWrapper*> mTileWrappers;
    QWidget *mDragIndicator;
    DashboardTileWrapper *mDragSource;

    QList<QLabel*> mSummaryLabels;
    QString mSummaryHostname;
    QString mSummaryOs;
    QString mSummaryCpu;
    QString mSummaryRam;

    void buildSystemSummary();
    void refreshSummaryColors();
    void updateDiskHealthBadge();
    void buildGrid();
    void rebuildLayout();
    DashboardTileWrapper *wrapTile(const QString &id, QWidget *tile);
    void applyDisplayModeForSpan(DashboardTileWrapper *wrapper);
    QJsonArray serializeLayout() const;
    void deserializeLayout(const QString &json);
    QJsonArray defaultLayout() const;
    int gridCellAtPos(const QPoint &globalPos, int &outRow, int &outCol) const;
};

#endif // DASHBOARDPAGE_H
