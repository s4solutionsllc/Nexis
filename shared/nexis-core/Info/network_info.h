#ifndef NETWORK_INFO_H
#define NETWORK_INFO_H

#include <QtNetwork/QNetworkInterface>
#include <QHash>
#include <QString>
#include "Utils/file_util.h"
#include "Utils/command_util.h"

#include "nexis-core_global.h"

// SSO-351 / GH#43: NetworkInfo now exposes a per-interface snapshot so
// NetUsageTracker can record stats for every up+running iface (Wi-Fi
// interfaces like wlp* were lost when only the first-scanned default was
// cached at construction). The default-iface fields (mRxBytes / mTxBytes /
// defaultNetworkInterface) remain for live-rate displays.
struct NetInterfaceStats {
    quint64 rx = 0;
    quint64 tx = 0;
};

// The bare QHash<QString, NetInterfaceStats> token cannot be passed straight
// into Q_DECLARE_METATYPE / signal signatures — the C preprocessor splits the
// comma into two macro arguments and the build breaks. Use this alias every
// time we need to refer to the per-interface map type so the metatype string
// matches end-to-end (declaration, signal, slot, qRegisterMetaType).
using NetInterfaceStatsMap = QHash<QString, NetInterfaceStats>;

class NEXISCORESHARED_EXPORT NetworkInfo
{
public:
    virtual ~NetworkInfo() = default;

    virtual QString getDefaultNetworkInterface() const = 0;
    virtual QList<QNetworkInterface> getAllInterfaces() = 0;

    virtual void updateNetworkBytes() = 0;
    quint64 getRXbytes() const { return mRxBytes; }
    quint64 getTXbytes() const { return mTxBytes; }

    // Snapshot populated by updateNetworkBytes() — keyed by interface name,
    // restricted to non-loopback up+running interfaces.
    const NetInterfaceStatsMap &getInterfaceStats() const { return mInterfaceStats; }

    // GH#191: human-readable name/type for an interface ("Wi-Fi", "Ethernet",
    // "Thunderbolt Bridge", "VPN", ...). Empty when none is available; callers
    // fall back to the raw kernel/BSD name.
    virtual QString interfaceDisplayName(const QString &name) const { return {}; }

protected:
    QString defaultNetworkInterface;
    quint64 mRxBytes = 0;
    quint64 mTxBytes = 0;
    NetInterfaceStatsMap mInterfaceStats;
};

Q_DECLARE_METATYPE(NetInterfaceStats)
Q_DECLARE_METATYPE(NetInterfaceStatsMap)

#endif // NETWORK_INFO_H
