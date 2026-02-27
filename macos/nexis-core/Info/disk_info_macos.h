#ifndef DISK_INFO_MACOS_H
#define DISK_INFO_MACOS_H

#include <Info/disk_info.h>
#include <QElapsedTimer>

class DiskInfoMacOS : public DiskInfo
{
public:
    QList<quint64> getDiskIO() const override;
    QStringList getDiskNames() const override;

private:
    mutable quint64 mPrevRead = 0;
    mutable quint64 mPrevWrite = 0;
    mutable QElapsedTimer mIoTimer;
    mutable bool mIoTimerStarted = false;
};

#endif // DISK_INFO_MACOS_H
