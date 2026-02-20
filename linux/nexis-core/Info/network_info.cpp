#include "network_info_linux.h"
#include <QDebug>

NetworkInfoLinux::NetworkInfoLinux()
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

quint64 NetworkInfoLinux::getRXbytes() const
{
    quint64 rx = FileUtil::readStringFromFile(rxPath)
            .trimmed()
            .toLongLong();

    return rx;
}

quint64 NetworkInfoLinux::getTXbytes() const
{
    quint64 tx = FileUtil::readStringFromFile(txPath)
            .trimmed()
            .toLongLong();

    return tx;
}

QList<QNetworkInterface> NetworkInfoLinux::getAllInterfaces()
{
    return QNetworkInterface::allInterfaces();
}

QString NetworkInfoLinux::getDefaultNetworkInterface() const
{
    return defaultNetworkInterface;
}
