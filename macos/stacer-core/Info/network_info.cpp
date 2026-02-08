#include "network_info.h"
#include <QDebug>

#include <net/if.h>
#include <net/if_dl.h>
#include <ifaddrs.h>

NetworkInfo::NetworkInfo()
{
    for (const QNetworkInterface &net: QNetworkInterface::allInterfaces()) {
        if ((net.flags()  & QNetworkInterface::IsUp) &&
            (net.flags()  & QNetworkInterface::IsRunning) &&
            !(net.flags() & QNetworkInterface::IsLoopBack))
        {
            defaultNetworkInterface = net.name();
            break;
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
