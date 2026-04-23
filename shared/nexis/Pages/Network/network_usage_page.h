#ifndef NETWORK_USAGE_PAGE_H
#define NETWORK_USAGE_PAGE_H

#include "nexis_page.h"
#include "net_usage_tracker.h"

#include <QLabel>
#include <QComboBox>
#include <QProgressBar>
#include <QSpinBox>
#include <QCheckBox>
#include <QFrame>

class DataRefreshService;

class NetworkUsagePage : public NexisPage
{
    Q_OBJECT

public:
    explicit NetworkUsagePage(QWidget *parent = nullptr,
                              DataRefreshService *drs = nullptr);

    void onPageActivated() override;
    void onPageDeactivated() override;

private slots:
    void onNetworkTick(quint64 rxAbs, quint64 txAbs);
    void onDataChanged();
    void onIfaceChanged(int index);
    void onCapChanged(int gb);
    void onResetDayChanged(int day);
    void onAlertToggled(bool enabled);
    void refreshThemeColors();

private:
    void buildUI();
    void refreshStats();
    void refreshCapBar();
    void refreshBarChart();
    void populateIfaceCombo();

    DataRefreshService *mDrs;

    QComboBox *mIfaceCombo;

    // Summary cards
    QLabel *mLblTodayVal;
    QLabel *mLblWeekVal;
    QLabel *mLblMonthVal;

    // Live rate
    QLabel *mLblRateDown;
    QLabel *mLblRateUp;
    quint64 mLastRx = 0;
    quint64 mLastTx = 0;
    bool mHaveLastRx = false;

    // Cap bar
    QFrame *mCapCard;
    QProgressBar *mCapBar;
    QLabel *mLblCapUsed;
    QLabel *mLblCapLimit;

    // Settings
    QSpinBox *mSpinCap;
    QSpinBox *mSpinResetDay;
    QCheckBox *mChkAlert;

    // Bar chart widget (defined in .cpp)
    class BarChartWidget *mBarChart;

    bool mActive = false;
};

#endif // NETWORK_USAGE_PAGE_H
