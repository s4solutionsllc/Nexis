#ifndef WOL_WIDGET_H
#define WOL_WIDGET_H

#include <QMap>
#include <QWidget>

class QFrame;
class QLabel;
class QPushButton;
class QTableWidget;
class QTableWidgetItem;

// FR-120: Wake-on-LAN helper. Cross-platform card on the Helpers page.
// Reads the ARP cache to discover local hosts, lets the user assign friendly
// names (persisted in SettingManager), and sends a UDP magic packet on demand.
// No root required — reading ARP and sending UDP broadcast is unprivileged.
struct WolHost {
    QString ip;
    QString mac;
    QString friendlyName;
};

class WolWidget : public QWidget
{
    Q_OBJECT

public:
    explicit WolWidget(QWidget *parent = nullptr);

    void loadIfNeeded();

signals:
    void hostsFetched(QList<WolHost> hosts);

private slots:
    void onDiscoverClicked();
    void onHostsFetched(QList<WolHost> hosts);
    void onItemChanged(QTableWidgetItem *item);
    void refreshThemeColors();

private:
    void buildUI();
    void sendMagicPacket(const QString &mac);
    void populateTable(const QList<WolHost> &hosts);
    void saveNames();
    void loadNames();

    QFrame        *mCard        = nullptr;
    QLabel        *mLblTitle    = nullptr;
    QLabel        *mLblStatus   = nullptr;
    QTableWidget  *mTable       = nullptr;
    QPushButton   *mBtnDiscover = nullptr;

    QMap<QString, QString> mFriendlyNames;  // MAC → friendly name
    bool mLoaded = false;
    bool mIgnoreItemChanged = false;

    static constexpr int ColIp   = 0;
    static constexpr int ColMac  = 1;
    static constexpr int ColName = 2;
    static constexpr int ColWake = 3;
};

#endif // WOL_WIDGET_H
