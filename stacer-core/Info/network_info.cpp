#include "network_info.h"
#include <QDebug>

#ifdef Q_OS_LINUX

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

    rxPath = QString("/sys/class/net/%1/statistics/rx_bytes")
            .arg(defaultNetworkInterface);

    txPath = QString("/sys/class/net/%1/statistics/tx_bytes")
            .arg(defaultNetworkInterface);
}

quint64 NetworkInfo::getRXbytes() const
{
    quint64 rx = FileUtil::readStringFromFile(rxPath)
            .trimmed()
            .toLong();

    return rx;
}

quint64 NetworkInfo::getTXbytes() const
{
    quint64 tx = FileUtil::readStringFromFile(txPath)
            .trimmed()
            .toLong();

    return tx;
}

#elif defined(Q_OS_MACOS)

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

#endif

QList<QNetworkInterface> NetworkInfo::getAllInterfaces()
{
    return QNetworkInterface::allInterfaces();
}

QString NetworkInfo::getDefaultNetworkInterface() const
{
    return defaultNetworkInterface;
}
