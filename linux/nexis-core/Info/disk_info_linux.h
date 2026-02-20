#ifndef DISK_INFO_LINUX_H
#define DISK_INFO_LINUX_H

#include <Info/disk_info.h>

class DiskInfoLinux : public DiskInfo
{
public:
    QList<quint64> getDiskIO() const override;
    QStringList getDiskNames() const override;
};

#endif // DISK_INFO_LINUX_H
