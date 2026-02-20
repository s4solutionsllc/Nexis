#ifndef DISKINFO_H
#define DISKINFO_H

#include "Utils/command_util.h"
#include "Utils/file_util.h"
#include <QStorageInfo>
#include <QSet>
#include "nexis-core_global.h"

struct Disk {
    QString name;
    QString device;
    QString fileSystemType;
    quint64 size = 0;
    quint64 free = 0;
    quint64 used = 0;
};

class NEXISCORESHARED_EXPORT DiskInfo
{
public:
    virtual ~DiskInfo() = default;

    QList<Disk> getDisks() const;
    void updateDiskInfo();
    virtual QList<quint64> getDiskIO() const = 0;
    virtual QStringList getDiskNames() const = 0;
    QList<QString> fileSystemTypes();
    QList<QString> devices();

protected:
    QList<Disk> disks;
};

#endif // DISKINFO_H
