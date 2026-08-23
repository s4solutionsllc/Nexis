#ifndef FIREWALL_WIDGET_H
#define FIREWALL_WIDGET_H

#include <QWidget>

class QFrame;
class QLabel;
class QPushButton;
class QToolButton;
struct ExecResult;

struct FirewallStatus {
    bool    available    = false;
    bool    enabled      = false;
    bool    stealthMode  = false;
    bool    blockAll     = false;
    int     appRuleCount = -1;
    QString backend;
    QString errorMsg;
};

class FirewallWidget : public QWidget
{
    Q_OBJECT

public:
    explicit FirewallWidget(QWidget *parent = nullptr);

    void loadIfNeeded();
    void refresh();

    static FirewallStatus parseMacFirewallOutput(const QString &globalState,
                                                  const QString &stealthMode,
                                                  const QString &blockAll,
                                                  const QString &listApps);
    static FirewallStatus parseUfwOutput(const QString &output);
    static FirewallStatus parseFirewalldOutput(const QString &output);

    // SSO-23402: `ufw status` requires root, so unprivileged callers get a
    // permission error with empty/unusable stdout and detection silently
    // reports "unavailable" even though ufw is installed and enabled. This
    // fallback determines enablement without a privileged call, using
    // `systemctl is-active ufw` output and the contents of /etc/ufw/ufw.conf
    // (both root-readable). Exposed for testing without shelling out.
    static FirewallStatus parseUfwFallback(const QString &systemctlIsActiveOutput,
                                            const QString &ufwConfContents);

    // SSO-3469: formats a sudoExecWithStatus() failure for display. Empty
    // string means the command succeeded (ExecResult::ok()). Exposed for
    // testing without shelling out to a real privileged command.
    static QString describeExecFailure(const ExecResult &r);

signals:
    void statusFetched(FirewallStatus status);
    void toggleFailed(QString error);

private slots:
    void onStatusFetched(FirewallStatus status);
    void onToggleFailed(const QString &error);
    void onToggleClicked();
    void refreshThemeColors();

private:
    FirewallStatus fetchStatus();
    QString toggleFirewall(bool enable);
    void buildUI();

    QLabel      *mLblTitle        = nullptr;
    QLabel      *mLblDot          = nullptr;
    QLabel      *mLblStatus       = nullptr;
    QPushButton *mBtnToggle       = nullptr;
    QLabel      *mLblBackend      = nullptr;
    QLabel      *mLblStealth      = nullptr;
    QLabel      *mLblBlockAll     = nullptr;
    QLabel      *mLblAppRules     = nullptr;
    QFrame      *mDetailWidget    = nullptr;
    QWidget     *mNotAvailWidget  = nullptr;
    QLabel      *mLblNotAvail     = nullptr;
    QToolButton *mBtnHelp         = nullptr;
    QPushButton *mBtnRefresh      = nullptr;
    QLabel      *mLblLoading      = nullptr;

    bool mLoaded = false;
    FirewallStatus mCurrentStatus;
};

#endif // FIREWALL_WIDGET_H
