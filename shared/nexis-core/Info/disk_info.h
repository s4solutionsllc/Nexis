#ifndef DISKINFO_H
#define DISKINFO_H

#include "Utils/command_util.h"
#include "Utils/file_util.h"
#include <QStorageInfo>
#include <QSet>
#include "nexis-core_global.h"

class Disk;

class NEXISCORESHARED_EXPORT DiskInfo
{
public:
    QList<Disk*> getDisks() const;
    void updateDiskInfo();
    QList<quint64> getDiskIO() const;
    QStringList getDiskNames() const;
    QList<QString> fileSystemTypes();
    QList<QString> devices();
    ~DiskInfo();

private:
    QList<Disk*> disks;
};

struct Disk {
    QString name;
    QString device;
    QString fileSystemType;
    quint64 size;
    quint64 free;
    quint64 used;
};


#endif // DISKINFO_H
