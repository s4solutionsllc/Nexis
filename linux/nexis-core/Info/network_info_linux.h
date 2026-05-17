#ifndef NETWORK_INFO_LINUX_H
#define NETWORK_INFO_LINUX_H

#include <Info/network_info.h>
#include <QByteArray>

class NetworkInfoLinux : public NetworkInfo
{
public:
    NetworkInfoLinux();

    QString getDefaultNetworkInterface() const override;
    QList<QNetworkInterface> getAllInterfaces() override;

    void updateNetworkBytes() override;

    // Exposed for unit tests. Parses the contents of /proc/net/route and
    // returns the iface name owning the IPv4 default route (Destination
    // column == 00000000), or an empty string if none.
    static QString parseDefaultRouteIface(const QByteArray &routeContents);

private:
    void refreshDefaultInterface();
};

#endif // NETWORK_INFO_LINUX_H
