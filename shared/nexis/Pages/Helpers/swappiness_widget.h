#ifndef SWAPPINESS_WIDGET_H
#define SWAPPINESS_WIDGET_H

#include <QWidget>

class QButtonGroup;
class QCheckBox;
class QFrame;
class QLabel;
class QPushButton;
class QSlider;

// FR-81: Linux-only Helpers-page card for tuning /proc/sys/vm/swappiness.
// Reads current value, surfaces presets (Desktop 60, Performance 10,
// Low-RAM 80, Custom N) plus an optional persistence toggle that writes
// /etc/sysctl.d/99-nexis-swappiness.conf.
struct SwappinessStatus {
    bool    available       = false;   // /proc/sys/vm/swappiness readable
    int     currentValue    = -1;
    bool    persisted       = false;   // sysctl.d/99-nexis-swappiness.conf exists
    int     persistedValue  = -1;
    quint64 swapTotalBytes  = 0;
    quint64 swapUsedBytes   = 0;
    int     swapBackendCount = 0;      // lines in /proc/swaps minus header
    QString errorMsg;
};

class SwappinessWidget : public QWidget
{
    Q_OBJECT

public:
    explicit SwappinessWidget(QWidget *parent = nullptr);

    void loadIfNeeded();
    void refresh();

signals:
    void statusFetched(SwappinessStatus status);

private slots:
    void onStatusFetched(SwappinessStatus status);
    void onPresetClicked(int preset);
    void onApplyClicked();
    void refreshThemeColors();

private:
    SwappinessStatus fetchStatus();
    bool applySwappiness(int value, bool persist);
    void buildUI();
    void renderStatus(const SwappinessStatus &s);
    int  selectedValue() const;
    void setPreset(int value);

    QLabel       *mLblTitle        = nullptr;
    QLabel       *mLblCurrent      = nullptr;
    QFrame       *mDetailWidget    = nullptr;
    QButtonGroup *mPresetGroup     = nullptr;
    QPushButton  *mBtnDesktop      = nullptr;
    QPushButton  *mBtnPerformance  = nullptr;
    QPushButton  *mBtnLowRam       = nullptr;
    QPushButton  *mBtnCustom       = nullptr;
    QSlider      *mSldCustom       = nullptr;
    QLabel       *mLblCustomValue  = nullptr;
    QCheckBox    *mChkPersist      = nullptr;
    QLabel       *mLblSwapUsage    = nullptr;
    QPushButton  *mBtnApply        = nullptr;
    QLabel       *mLblNotAvail     = nullptr;
    QLabel       *mLblLoading      = nullptr;
    QLabel       *mLblResult       = nullptr;
    QPushButton  *mBtnRefresh      = nullptr;

    bool mLoaded = false;
    SwappinessStatus mCurrent;
};

#endif // SWAPPINESS_WIDGET_H
