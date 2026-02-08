#ifndef NETWORK_INFO_H
#define NETWORK_INFO_H

#include <QtNetwork/QNetworkInterface>
#include "Utils/file_util.h"
#include "Utils/command_util.h"

#include "stacer-core_global.h"

class STACERCORESHARED_EXPORT NetworkInfo
{
public:
    NetworkInfo();

    QString getDefaultNetworkInterface() const;
    QList<QNetworkInterface> getAllInterfaces();

    quint64 getRXbytes() const;
    quint64 getTXbytes() const;

private:
    QString defaultNetworkInterface;

#ifdef Q_OS_LINUX
    QString rxPath;
    QString txPath;
#endif
};

#endif // NETWORK_INFO_H
