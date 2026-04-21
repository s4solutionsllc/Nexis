#ifndef CPU_TUNING_WIDGET_H
#define CPU_TUNING_WIDGET_H

#include <QtGlobal>

// FR-117: Linux-only Helpers-page card. Turbo boost, min/max frequency
// sliders (whole-CPU), and a per-core governor grid ("Advanced" expandable).
// Reads sysfs for state; writes via pkexec. Persists optionally on app
// launch via SettingManager toggles.
//
// The entire class is compiled out on macOS — callers guard their uses
// with #ifdef Q_OS_LINUX.
#ifdef Q_OS_LINUX

#include <QList>
#include <QWidget>

#include "Info/cpu_tuning.h"

class QCheckBox;
class QComboBox;
class QFrame;
class QLabel;
class QPushButton;
class QSlider;

class CpuTuningWidget : public QWidget
{
    Q_OBJECT

public:
    explicit CpuTuningWidget(QWidget *parent = nullptr);

    void loadIfNeeded();
    void refresh();

    // Re-applies saved settings at app launch when CpuTuningPersist is on.
    // Called from App::init after the landing page is clicked.
    static void applyPersistedSettings();

signals:
    void snapshotFetched(CpuTuning::Snapshot snap);

private slots:
    void onSnapshotFetched(CpuTuning::Snapshot snap);
    void onApplyClicked();
    void onAdvancedToggled(bool on);
    void refreshThemeColors();

private:
    CpuTuning::Snapshot fetchSnapshot();
    void buildUI();
    void renderSnapshot(const CpuTuning::Snapshot &snap);
    void buildPerCoreGrid(const CpuTuning::Snapshot &snap);

    QLabel       *mLblTitle        = nullptr;
    QLabel       *mLblDriver       = nullptr;
    QLabel       *mLblConflict     = nullptr;
    QLabel       *mLblPPDNotice    = nullptr;
    QFrame       *mCard            = nullptr;

    QCheckBox    *mChkTurbo        = nullptr;
    QLabel       *mLblTurbo        = nullptr;

    QSlider      *mSldMin          = nullptr;
    QSlider      *mSldMax          = nullptr;
    QLabel       *mLblMinVal       = nullptr;
    QLabel       *mLblMaxVal       = nullptr;

    QComboBox    *mCmbGovernor     = nullptr;
    QCheckBox    *mChkAdvanced     = nullptr;
    QWidget      *mAdvancedPanel   = nullptr;
    QFrame       *mPerCoreGrid     = nullptr;
    QList<QComboBox*> mCoreGovernorCombos;

    QCheckBox    *mChkPersist      = nullptr;
    QPushButton  *mBtnApply        = nullptr;
    QPushButton  *mBtnRefresh      = nullptr;
    QLabel       *mLblLoading      = nullptr;
    QLabel       *mLblResult       = nullptr;
    QLabel       *mLblNotAvail     = nullptr;

    bool mLoaded = false;
    bool mPPDBackend = false;
    CpuTuning::Snapshot mCurrent;
};

#endif // Q_OS_LINUX

#endif // CPU_TUNING_WIDGET_H
