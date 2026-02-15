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
    QList<Disk> getDisks() const;
    void updateDiskInfo();
    QList<quint64> getDiskIO() const;
    QStringList getDiskNames() const;
    QList<QString> fileSystemTypes();
    QList<QString> devices();

private:
    QList<Disk> disks;
};


#endif // DISKINFO_H
