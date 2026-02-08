#include "network_info.h"
#include "command_util.h"
#include <QDebug>
#include <QRegularExpression>

#include <net/if.h>
#include <net/if_dl.h>
#include <ifaddrs.h>

NetworkInfo::NetworkInfo()
{
    // On macOS many virtual interfaces (anpi, bridge, awdl, utun) are
    // Up+Running but carry no real traffic.  Ask the routing table for
    // the interface that owns the default route – that's the one whose
    // bandwidth the user actually cares about.
    try {
        QString out = CommandUtil::exec("route", {"-n", "get", "default"});
        QRegularExpression re(R"(interface:\s*(\S+))");
        QRegularExpressionMatch m = re.match(out);
        if (m.hasMatch()) {
            defaultNetworkInterface = m.captured(1);   // e.g. "en0"
        }
    } catch (...) {}

    // Fallback: first interface with an IPv4/IPv6 address that isn't loopback
    if (defaultNetworkInterface.isEmpty()) {
        for (const QNetworkInterface &net : QNetworkInterface::allInterfaces()) {
            if ((net.flags()  & QNetworkInterface::IsUp) &&
                (net.flags()  & QNetworkInterface::IsRunning) &&
                !(net.flags() & QNetworkInterface::IsLoopBack) &&
                !net.addressEntries().isEmpty())
            {
                defaultNetworkInterface = net.name();
                break;
            }
        }
    }
}

quint64 NetworkInfo::getRXbytes() const
{
    quint64 rx = 0;
    struct ifaddrs *ifap, *ifa;

    if (getifaddrs(&ifap) == 0) {
        for (ifa = ifap; ifa; ifa = ifa->ifa_next) {
            if (ifa->ifa_addr && ifa->ifa_addr->sa_family == AF_LINK &&
                QString(ifa->ifa_name) == defaultNetworkInterface)
            {
                struct if_data *ifData = static_cast<struct if_data *>(ifa->ifa_data);
                if (ifData) {
                    rx = ifData->ifi_ibytes;
                }
                break;
            }
        }
        freeifaddrs(ifap);
    }
    return rx;
}

quint64 NetworkInfo::getTXbytes() const
{
    quint64 tx = 0;
    struct ifaddrs *ifap, *ifa;

    if (getifaddrs(&ifap) == 0) {
        for (ifa = ifap; ifa; ifa = ifa->ifa_next) {
            if (ifa->ifa_addr && ifa->ifa_addr->sa_family == AF_LINK &&
                QString(ifa->ifa_name) == defaultNetworkInterface)
            {
                struct if_data *ifData = static_cast<struct if_data *>(ifa->ifa_data);
                if (ifData) {
                    tx = ifData->ifi_obytes;
                }
                break;
            }
        }
        freeifaddrs(ifap);
    }
    return tx;
}

QList<QNetworkInterface> NetworkInfo::getAllInterfaces()
{
    return QNetworkInterface::allInterfaces();
}

QString NetworkInfo::getDefaultNetworkInterface() const
{
    return defaultNetworkInterface;
}
