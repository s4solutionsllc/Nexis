#include "network_info_macos.h"
#include "command_util.h"
#include <QDebug>
#include <QRegularExpression>

#include <net/if.h>
#include <net/if_dl.h>
#include <ifaddrs.h>

#include <SystemConfiguration/SystemConfiguration.h>

NetworkInfoMacOS::NetworkInfoMacOS()
{
    refreshDefaultInterface();
}

void NetworkInfoMacOS::refreshDefaultInterface()
{
    // On macOS many virtual interfaces (anpi, bridge, awdl, utun) are
    // Up+Running but carry no real traffic.  Ask the routing table for
    // the interface that owns the default route – that's the one whose
    // bandwidth the user actually cares about.
    QString chosen;
    try {
        QString out = CommandUtil::exec("route", {"-n", "get", "default"});
        QRegularExpression re(R"(interface:\s*(\S+))");
        QRegularExpressionMatch m = re.match(out);
        if (m.hasMatch())
            chosen = m.captured(1); // e.g. "en0"
    } catch (...) { qWarning() << "Failed to detect default network interface"; }

    // Fallback: first interface with an IPv4/IPv6 address that isn't loopback
    if (chosen.isEmpty()) {
        for (const QNetworkInterface &net : QNetworkInterface::allInterfaces()) {
            if ((net.flags()  & QNetworkInterface::IsUp) &&
                (net.flags()  & QNetworkInterface::IsRunning) &&
                !(net.flags() & QNetworkInterface::IsLoopBack) &&
                !net.addressEntries().isEmpty())
            {
                chosen = net.name();
                break;
            }
        }
    }

    defaultNetworkInterface = chosen;
}

void NetworkInfoMacOS::updateNetworkBytes()
{
    // SSO-351: re-enumerate every tick and populate per-iface stats so
    // NetUsageTracker can record every active interface, not just the
    // default one cached at construction.
    refreshDefaultInterface();
    mInterfaceStats.clear();

    struct ifaddrs *ifap = nullptr;
    if (getifaddrs(&ifap) != 0) {
        mRxBytes = 0;
        mTxBytes = 0;
        return;
    }

    for (struct ifaddrs *ifa = ifap; ifa; ifa = ifa->ifa_next) {
        if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_LINK)
            continue;
        if ((ifa->ifa_flags & IFF_LOOPBACK)
            || !(ifa->ifa_flags & IFF_UP)
            || !(ifa->ifa_flags & IFF_RUNNING))
            continue;

        struct if_data *ifData = static_cast<struct if_data *>(ifa->ifa_data);
        if (!ifData)
            continue;

        NetInterfaceStats s;
        s.rx = ifData->ifi_ibytes;
        s.tx = ifData->ifi_obytes;
        mInterfaceStats.insert(QString::fromUtf8(ifa->ifa_name), s);
    }

    freeifaddrs(ifap);

    const auto it = mInterfaceStats.constFind(defaultNetworkInterface);
    if (it != mInterfaceStats.constEnd()) {
        mRxBytes = it->rx;
        mTxBytes = it->tx;
    } else {
        mRxBytes = 0;
        mTxBytes = 0;
    }
}

QList<QNetworkInterface> NetworkInfoMacOS::getAllInterfaces()
{
    return QNetworkInterface::allInterfaces();
}

QString NetworkInfoMacOS::getDefaultNetworkInterface() const
{
    return defaultNetworkInterface;
}

void NetworkInfoMacOS::rebuildDisplayNameCache() const
{
    // GH#191: walk every configured network interface once and map its BSD
    // name (e.g. "en0") to the OS-localized display name (e.g. "Wi-Fi").
    mDisplayNameCache.clear();

    CFArrayRef interfaces = SCNetworkInterfaceCopyAll();
    if (!interfaces)
        return;

    const CFIndex count = CFArrayGetCount(interfaces);
    for (CFIndex i = 0; i < count; ++i) {
        SCNetworkInterfaceRef iface =
            static_cast<SCNetworkInterfaceRef>(
                const_cast<void *>(CFArrayGetValueAtIndex(interfaces, i)));
        if (!iface)
            continue;

        CFStringRef bsdName = SCNetworkInterfaceGetBSDName(iface);
        CFStringRef displayName = SCNetworkInterfaceGetLocalizedDisplayName(iface);
        if (!bsdName || !displayName)
            continue;

        mDisplayNameCache.insert(QString::fromCFString(bsdName),
                                 QString::fromCFString(displayName));
    }

    CFRelease(interfaces);
}

QString NetworkInfoMacOS::interfaceDisplayName(const QString &name) const
{
    // Called on the network tile subtitle every ~1s — keep it cheap by
    // caching. Populate lazily on first use; if the queried name is missing
    // (interface plugged in after the cache was built), rebuild once.
    if (mDisplayNameCache.isEmpty())
        rebuildDisplayNameCache();

    auto it = mDisplayNameCache.constFind(name);
    if (it == mDisplayNameCache.constEnd()) {
        rebuildDisplayNameCache();
        it = mDisplayNameCache.constFind(name);
    }

    return it != mDisplayNameCache.constEnd() ? it.value() : QString();
}
