#ifndef BATTERY_CHARGE_THRESHOLD_WIDGET_H
#define BATTERY_CHARGE_THRESHOLD_WIDGET_H

// FW-15 (SSO-3743): Linux-only Helpers-page card for charge threshold control.
// Gate on charge_control_end_threshold existing; hidden when unsupported.

#include <QWidget>

#ifdef Q_OS_LINUX
#include "Info/battery_charge_threshold.h"

class QButtonGroup;
class QCheckBox;
class QFrame;
class QLabel;
class QPushButton;
class QSlider;

class BatteryChargeThresholdWidget : public QWidget
{
    Q_OBJECT

public:
    explicit BatteryChargeThresholdWidget(QWidget *parent = nullptr);

    void loadIfNeeded();
    void refresh();

signals:
    void statusFetched(ChargeThresholdStatus status);

private slots:
    void onStatusFetched(ChargeThresholdStatus status);
    void onPresetClicked(int preset);
    void onApplyClicked();
    void refreshThemeColors();

private:
    ChargeThresholdStatus fetchStatus();
    void buildUI();
    void renderStatus(const ChargeThresholdStatus &s);
    int  selectedEndPct() const;

    QLabel       *mLblTitle       = nullptr;
    QLabel       *mLblCurrent     = nullptr;
    QFrame       *mDetailWidget   = nullptr;
    QButtonGroup *mPresetGroup    = nullptr;
    QPushButton  *mBtnMaximize    = nullptr;
    QPushButton  *mBtnPreserve    = nullptr;
    QPushButton  *mBtnCustom      = nullptr;
    QSlider      *mSldCustom      = nullptr;
    QLabel       *mLblCustomValue = nullptr;
    QCheckBox    *mChkPersist     = nullptr;
    QPushButton  *mBtnApply       = nullptr;
    QLabel       *mLblNotAvail    = nullptr;
    QLabel       *mLblLoading     = nullptr;
    QLabel       *mLblResult      = nullptr;
    QPushButton  *mBtnRefresh     = nullptr;

    bool mLoaded = false;
    ChargeThresholdStatus mCurrent;
};

#endif // Q_OS_LINUX
#endif // BATTERY_CHARGE_THRESHOLD_WIDGET_H
