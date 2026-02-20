#ifndef NETWORK_INFO_MACOS_H
#define NETWORK_INFO_MACOS_H

#include <Info/network_info.h>

class NetworkInfoMacOS : public NetworkInfo
{
public:
    NetworkInfoMacOS();

    QString getDefaultNetworkInterface() const override;
    QList<QNetworkInterface> getAllInterfaces() override;

    quint64 getRXbytes() const override;
    quint64 getTXbytes() const override;
};

#endif // NETWORK_INFO_MACOS_H
