#ifndef DASHBOARDPAGE_H
#define DASHBOARDPAGE_H

#include <QMenu>
#include <QPushButton>
#include <QToolButton>
#include <QLabel>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSet>
#include <QDesktopServices>
#include <QShortcut>
#include <QtConcurrent>

#include "nexis_page.h"
#include "Managers/info_manager.h"
#include "metric_tile_base.h"
#include "metric_tile.h"
#include "network_tile.h"
#include "disk_tile.h"
#include "gauge_tile.h"
#include "ring_tile.h"
#include "hybrid_tile.h"
#include "speedometer_tile.h"
#include "vumeter_tile.h"
#include "health_score_tile.h"
#include "dashboard_tile_wrapper.h"

#include "Managers/setting_manager.h"

class AppManager;
class SignalMapper;
class DataRefreshService;

namespace Ui {
    class DashboardPage;
}

class DashboardPage : public NexisPage
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

    void onPageActivated() override;
    void onPageDeactivated() override;

protected:
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void onKioskModeChanged(bool enabled);
    void init();
    void checkUpdate();

    void onCpuUpdated(const QList<int> &percents, double clockGHz, const QList<double> &loadAvgs);
    void onMemoryUpdated(const MemorySnapshot &snap);
    void onNetworkUpdated(quint64 rxBytes, quint64 txBytes);
    void onDiskUsageUpdated(const QList<Disk> &disks);
    void updateTempTile();
    void updateFanTile();
    void onFanSensorSelected(QAction *action);
    void onGpuUpdated(const QList<GpuDevice> &gpus);
    void onTempSensorSelected(QAction *action);
    void onGpuDeviceSelected(QAction *action);
    void onDiskSelected(QAction *action);
    void onBatteryUpdated(const BatteryData &bat);
    void onDiskHealthUpdated(const QList<DriveHealth> &drives);

    void onHealthCpuUpdated(const QList<int> &percents, double clockGHz, const QList<double> &loadAvgs);
    void onHealthMemoryUpdated(const MemorySnapshot &snap);
    void onHealthDiskUpdated(const QList<Disk> &disks);
    void onHealthTempUpdated();
    void onHealthBatteryUpdated(const BatteryData &bat);
    void onHealthDiskHealthUpdated(const QList<DriveHealth> &drives);

    void on_btnDownloadUpdate_clicked();
    void toggleEditMode();
    void exitEditMode();
    void onResetLayout();
    void onTileDragStarted(DashboardTileWrapper *wrapper, const QPoint &globalPos);
    void onTileDragMoved(DashboardTileWrapper *wrapper, const QPoint &globalPos);
    void onTileDragFinished(DashboardTileWrapper *wrapper, const QPoint &globalPos);
    void onTileResizeRequested(DashboardTileWrapper *wrapper, int newColSpan, int newRowSpan);
    void onTileStyleChangeRequested(DashboardTileWrapper *wrapper, const QString &style);
    void onTileColorChangeRequested(DashboardTileWrapper *wrapper, const QString &hexColor);
    void onTileRangeChangeRequested(DashboardTileWrapper *wrapper, const QString &rangeId);
    void onTileRemoveRequested(DashboardTileWrapper *wrapper);

signals:
    void sigShowUpdateBar();

private:
    Ui::DashboardPage *ui;

    MetricTileBase *mCpuTile;
    MetricTileBase *mMemTile;
    MetricTileBase *mDiskTile;
    MetricTileBase *mTempTile;
    MetricTileBase *mGpuTile;
    MetricTileBase *mBatteryTile;
    MetricTileBase *mFanTile;
    HealthScoreTile *mHealthTile;
    NetworkTile *mNetworkTile;

    QMenu *mGpuDeviceMenu;
    QMenu *mDiskMenu;
    QMenu *mTempSensorMenu;
    QMenu *mFanSensorMenu;
    QList<Disk> mCachedDisks;
    QList<DriveHealth> mCachedDriveHealth;

    InfoManager *im;
    SettingManager *mSettingManager;
    AppManager *mAppManager;
    SignalMapper *mSignalMapper;
    DataRefreshService *mRefresh;

    int mSelectedSensorIndex;
    int mSelectedGpuIndex;
    int mSelectedFanIndex;

    QToolButton *mKioskButton;
    QToolButton *mEditButton;
    QWidget *mEditToolbar;
    QPushButton *mBtnResetLayout;
    QPushButton *mBtnDone;
    QShortcut *mEditShortcut;
    bool mEditMode;
    bool mKioskMode;

    static const int GRID_ROWS = 4;
    static const int GRID_COLS = 4;
    QString mOccupancy[GRID_ROWS][GRID_COLS];
    QList<QWidget*> mPlaceholders;

    QList<DashboardTileWrapper*> mTileWrappers;
    QWidget *mDragIndicator;
    DashboardTileWrapper *mDragSource;

    QMap<QString, QString> mTileStyles;
    QMap<QString, QString> mTileColors;
    QMap<QString, QString> mTileRanges;
    QSet<QString> mHiddenTiles;
    QSet<QString> mGearVisibleTiles;

    bool mActive;

    QString mCpuSubtitleBase;

    QList<QLabel*> mSummaryLabels;
    QString mSummaryHostname;
    QString mSummaryOs;
    QString mSummaryCpu;
    QString mSummaryRam;

    void applyFooterVisibility();
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
    bool gridCellAtPos(const QPoint &globalPos, int &outRow, int &outCol) const;
    void rebuildOccupancy();
    bool regionIsFree(int row, int col, int rowSpan, int colSpan,
                      const QString &ignoreTileId = QString()) const;

    MetricTileBase *createTile(const QString &id, const QString &style);
    QStringList availableStyles(const QString &tileId) const;
    QString defaultStyle(const QString &tileId) const;
    void tileTitle(const QString &id, QString &title, QString &colorToken) const;
    void setupTileGearMenu(const QString &id, MetricTileBase *tile);
    DashboardTileWrapper *findWrapper(const QString &tileId) const;
    bool tileUsesRangeMenu(const QString &style) const;
    void setupCustomizationMenu(DashboardTileWrapper *wrapper, const QString &style);
    void launchMaintenanceWizard();
};

#endif // DASHBOARDPAGE_H
