#ifndef TRIM_WIDGET_H
#define TRIM_WIDGET_H

#include <QWidget>

class QFrame;
class QLabel;
class QPlainTextEdit;
class QPushButton;

// FR-118: SSD TRIM scheduler integration. Cross-platform card.
// Linux: toggle fstrim.timer, run `fstrim -av` on demand, show
// last-run / next-run stamps parsed from `systemctl list-timers`.
// macOS: read-only status — APFS manages TRIM automatically and
// there's no safe user-space toggle.
struct TrimStatus {
    bool    available    = false;   // false → card shows "not supported" message
    bool    timerEnabled = false;   // Linux: is fstrim.timer enabled
    bool    timerActive  = false;   // Linux: is fstrim.timer currently active
    QString lastRun;                // Linux: parsed from list-timers, or empty
    QString nextRun;                // Linux: parsed from list-timers, or empty
    bool    trimEnabled  = false;   // macOS: OS reports TRIM on root volume
    QString platformNote;           // optional extra line ("APFS", driver name, etc.)
    QString errorMsg;
};

class TrimWidget : public QWidget
{
    Q_OBJECT

public:
    explicit TrimWidget(QWidget *parent = nullptr);

    void loadIfNeeded();
    void refresh();

signals:
    void statusFetched(TrimStatus status);

private slots:
    void onStatusFetched(TrimStatus status);
    void onToggleTimer();
    void onRunNow();
    void refreshThemeColors();

private:
    TrimStatus fetchStatus();
#ifdef Q_OS_LINUX
    bool toggleTimer(bool enable);
    QString runFstrimNow();
#endif
    void buildUI();
    void renderStatus(const TrimStatus &s);

    QLabel       *mLblTitle        = nullptr;
    QFrame       *mCard            = nullptr;
    QLabel       *mLblDot          = nullptr;
    QLabel       *mLblStatus       = nullptr;
    QLabel       *mLblLastRun      = nullptr;
    QLabel       *mLblNextRun      = nullptr;
    QLabel       *mLblPlatform     = nullptr;
    QPushButton  *mBtnToggle       = nullptr;
    QPushButton  *mBtnRunNow       = nullptr;
    QLabel       *mLblLoading      = nullptr;
    QLabel       *mLblResult       = nullptr;
    QPlainTextEdit *mTxtOutput     = nullptr;
    QPushButton  *mBtnRefresh      = nullptr;

    bool mLoaded = false;
    TrimStatus mCurrent;
};

#endif // TRIM_WIDGET_H
