#ifndef FIREWALL_WIDGET_H
#define FIREWALL_WIDGET_H

#include <QWidget>

class QFrame;
class QLabel;
class QPushButton;
class QToolButton;

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

signals:
    void statusFetched(FirewallStatus status);

private slots:
    void onStatusFetched(FirewallStatus status);
    void onToggleClicked();
    void refreshThemeColors();

private:
    FirewallStatus fetchStatus();
    void toggleFirewall(bool enable);
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
