#ifndef NETWORK_INFO_MACOS_H
#define NETWORK_INFO_MACOS_H

#include <Info/network_info.h>

class NetworkInfoMacOS : public NetworkInfo
{
public:
    NetworkInfoMacOS();

    QString getDefaultNetworkInterface() const override;
    QList<QNetworkInterface> getAllInterfaces() override;

    void updateNetworkBytes() override;
};

#endif // NETWORK_INFO_MACOS_H
