#include "network_info_linux.h"

#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QObject>
#include <QStringList>

namespace {

bool isUsableInterface(const QNetworkInterface &net)
{
    const auto flags = net.flags();
    return (flags & QNetworkInterface::IsUp)
        && (flags & QNetworkInterface::IsRunning)
        && !(flags & QNetworkInterface::IsLoopBack);
}

} // namespace

QString NetworkInfoLinux::parseDefaultRouteIface(const QByteArray &routeContents)
{
    // /proc/net/route format: header row, then whitespace-separated columns
    //   Iface  Destination  Gateway  Flags  RefCnt  Use  Metric  Mask ...
    // The IPv4 default route is the row whose Destination is "00000000".
    const QList<QByteArray> lines = routeContents.split('\n');
    for (int i = 0; i < lines.size(); ++i) {
        const QByteArray line = lines.at(i).trimmed();
        if (i == 0 || line.isEmpty())
            continue;
        const QList<QByteArray> cols = line.simplified().split(' ');
        if (cols.size() < 2)
            continue;
        if (cols.at(1) == "00000000")
            return QString::fromLatin1(cols.at(0));
    }
    return QString();
}

static QString readDefaultRouteIfaceFromProc()
{
    QFile f(QStringLiteral("/proc/net/route"));
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return QString();
    return NetworkInfoLinux::parseDefaultRouteIface(f.readAll());
}

NetworkInfoLinux::NetworkInfoLinux()
{
    refreshDefaultInterface();
}

void NetworkInfoLinux::refreshDefaultInterface()
{
    // Prefer the iface owning the IPv4 default route — that is what the
    // user's traffic actually flows over. This sidesteps the historic
    // "first up iface wins" bug where docker0 / virbr0 (or any other
    // virtual iface enumerated ahead of wlp*) shadowed the real default.
    QString chosen = readDefaultRouteIfaceFromProc();

    if (chosen.isEmpty()) {
        for (const QNetworkInterface &net : QNetworkInterface::allInterfaces()) {
            if (isUsableInterface(net)) {
                chosen = net.name();
                break;
            }
        }
    }

    defaultNetworkInterface = chosen;
}

void NetworkInfoLinux::updateNetworkBytes()
{
    // SSO-351: re-enumerate every tick so an interface that came up after
    // startup (e.g. Wi-Fi connecting later) starts being tracked.
    refreshDefaultInterface();

    mInterfaceStats.clear();

    for (const QNetworkInterface &net : QNetworkInterface::allInterfaces()) {
        if (!isUsableInterface(net))
            continue;

        const QString name = net.name();
        const QString rxPath = QStringLiteral("/sys/class/net/%1/statistics/rx_bytes").arg(name);
        const QString txPath = QStringLiteral("/sys/class/net/%1/statistics/tx_bytes").arg(name);

        bool ok = false;
        const quint64 rx = FileUtil::readStringFromFile(rxPath).trimmed().toULongLong(&ok);
        if (!ok)
            continue;
        const quint64 tx = FileUtil::readStringFromFile(txPath).trimmed().toULongLong(&ok);
        if (!ok)
            continue;

        NetInterfaceStats s;
        s.rx = rx;
        s.tx = tx;
        mInterfaceStats.insert(name, s);
    }

    // Expose the default iface's counters via the legacy accessors so the
    // live-rate displays (Dashboard, Resources, Network Usage rate card)
    // keep working unchanged.
    const auto it = mInterfaceStats.constFind(defaultNetworkInterface);
    if (it != mInterfaceStats.constEnd()) {
        mRxBytes = it->rx;
        mTxBytes = it->tx;
    } else {
        mRxBytes = 0;
        mTxBytes = 0;
    }
}

QList<QNetworkInterface> NetworkInfoLinux::getAllInterfaces()
{
    return QNetworkInterface::allInterfaces();
}

QString NetworkInfoLinux::getDefaultNetworkInterface() const
{
    return defaultNetworkInterface;
}

QString NetworkInfoLinux::interfaceDisplayName(const QString &name) const
{
    const QString base = QStringLiteral("/sys/class/net/%1").arg(name);
    if (QFileInfo::exists(base + "/wireless") || QFileInfo::exists(base + "/phy80211"))
        return QObject::tr("Wi-Fi");
    if (QFileInfo::exists(base + "/tun_flags"))
        return QObject::tr("VPN");
    if (QFileInfo::exists(base + "/bridge"))
        return QObject::tr("Bridge");
    QFile tf(base + "/type");
    if (tf.open(QIODevice::ReadOnly)) {
        const QByteArray t = tf.readAll().trimmed();
        if (t == "1") return QObject::tr("Ethernet");  // ARPHRD_ETHER
    }
    return {};
}
