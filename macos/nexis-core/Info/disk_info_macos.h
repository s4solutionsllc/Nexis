#ifndef DISK_INFO_MACOS_H
#define DISK_INFO_MACOS_H

#include <Info/disk_info.h>

class DiskInfoMacOS : public DiskInfo
{
public:
    QList<quint64> getDiskIO() const override;
    QStringList getDiskNames() const override;
};

#endif // DISK_INFO_MACOS_H
