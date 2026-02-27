#ifndef NETWORK_INFO_LINUX_H
#define NETWORK_INFO_LINUX_H

#include <Info/network_info.h>

class NetworkInfoLinux : public NetworkInfo
{
public:
    NetworkInfoLinux();

    QString getDefaultNetworkInterface() const override;
    QList<QNetworkInterface> getAllInterfaces() override;

    void updateNetworkBytes() override;

private:
    QString rxPath;
    QString txPath;
};

#endif // NETWORK_INFO_LINUX_H
