#ifndef NETWORK_INFO_MACOS_H
#define NETWORK_INFO_MACOS_H

#include <Info/network_info.h>
#include <QHash>

class NetworkInfoMacOS : public NetworkInfo
{
public:
    NetworkInfoMacOS();

    QString getDefaultNetworkInterface() const override;
    QList<QNetworkInterface> getAllInterfaces() override;

    void updateNetworkBytes() override;

    // GH#191: localized interface display name via SystemConfiguration.
    QString interfaceDisplayName(const QString &name) const override;

private:
    void refreshDefaultInterface();

    // GH#191: BSD name -> localized display name cache. Populated lazily on
    // first query; rebuilt once if a queried name is missing (interfaces can
    // be plugged/unplugged at runtime). Mutable so the const accessor can
    // refresh it.
    void rebuildDisplayNameCache() const;
    mutable QHash<QString, QString> mDisplayNameCache;
    mutable bool mDisplayNameCacheBuilt = false;
};

#endif // NETWORK_INFO_MACOS_H
