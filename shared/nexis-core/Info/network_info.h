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

    virtual quint64 getRXbytes() const = 0;
    virtual quint64 getTXbytes() const = 0;

protected:
    QString defaultNetworkInterface;
};

#endif // NETWORK_INFO_H
