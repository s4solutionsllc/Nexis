#ifndef NETWORK_INFO_H
#define NETWORK_INFO_H

#include <QtNetwork/QNetworkInterface>
#include "Utils/file_util.h"
#include "Utils/command_util.h"

#include "nexis-core_global.h"

class NEXISCORESHARED_EXPORT NetworkInfo
{
public:
    virtual ~NetworkInfo() = default;

    virtual QString getDefaultNetworkInterface() const = 0;
    virtual QList<QNetworkInterface> getAllInterfaces() = 0;

    virtual void updateNetworkBytes() = 0;
    quint64 getRXbytes() const { return mRxBytes; }
    quint64 getTXbytes() const { return mTxBytes; }

protected:
    QString defaultNetworkInterface;
    quint64 mRxBytes = 0;
    quint64 mTxBytes = 0;
};

#endif // NETWORK_INFO_H
