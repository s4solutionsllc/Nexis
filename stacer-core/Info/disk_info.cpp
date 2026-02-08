#include "disk_info.h"
#include <QDebug>
#include <QRegularExpression>

QList<Disk*> DiskInfo::getDisks() const
{
    return disks;
}

void DiskInfo::updateDiskInfo()
{
    qDeleteAll(disks);
    disks.clear();

    QList<QStorageInfo> storageInfoList = QStorageInfo::mountedVolumes();

    for(const QStorageInfo &info: storageInfoList) {
        if (info.isValid()) {
            Disk *disk = new Disk();
            disk->name = info.displayName();
            disk->device = info.device();
            disk->size = info.bytesTotal();
            disk->used = info.bytesTotal() - info.bytesFree();
            disk->free = info.bytesFree();
            disk->fileSystemType = info.fileSystemType();

            disks << disk;
        }
    }
}

QList<QString> DiskInfo::devices()
{
    QSet<QString> set;
    for(const QStorageInfo &info: QStorageInfo::mountedVolumes()) {
        if (info.isValid()) set.insert(info.device());
    }

    return set.values();
}

DiskInfo::~DiskInfo()
{
    qDeleteAll(disks);
}

QList<QString> DiskInfo::fileSystemTypes()
{
    QSet<QString> set;
    for(const QStorageInfo &info: QStorageInfo::mountedVolumes()) {
        if (info.isValid()) set.insert(info.fileSystemType());
    }

    return set.values();
}

#ifdef Q_OS_LINUX

QList<quint64> DiskInfo::getDiskIO() const
{
    static QStringList diskNames = getDiskNames();

    QList<quint64> diskReadWrite;
    quint64 totalRead = 0;
    quint64 totalWrite = 0;

    for (const QString diskName : diskNames) {
      QStringList diskStat = FileUtil::readStringFromFile(QString("/sys/block/%1/stat").arg(diskName))
              .trimmed()
              .split(QRegularExpression("\\s+"));

      if (diskStat.count() > 7) {
          totalRead = totalRead + (diskStat.at(2).toLongLong() * 512);
          totalWrite = totalWrite + (diskStat.at(6).toLongLong() * 512);
      }
    }
    diskReadWrite.append(totalRead);
    diskReadWrite.append(totalWrite);

    return diskReadWrite;
}

QStringList DiskInfo::getDiskNames() const
{
    QDir blocks("/sys/block");
    QStringList disks;
    for (const QFileInfo entryInfo : blocks.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot)) {
        if (QFile::exists(QString("%1/device").arg(entryInfo.absoluteFilePath()))) {
            disks.append(entryInfo.baseName());
        }
    }
    return disks;
}

#elif defined(Q_OS_MACOS)

QList<quint64> DiskInfo::getDiskIO() const
{
    QList<quint64> diskReadWrite;
    quint64 totalRead = 0;
    quint64 totalWrite = 0;

    // On macOS, use iostat to get disk I/O statistics
    try {
        QString output = CommandUtil::exec("iostat", {"-d", "-c", "1", "-w", "1"});
        // iostat output format varies; parse the totals from the last line
        QStringList lines = output.trimmed().split('\n');
        if (lines.size() >= 3) {
            // Skip header lines, parse last data line
            QStringList parts = lines.last().trimmed().split(QRegularExpression("\\s+"));
            if (parts.size() >= 3) {
                // KB/t, tps, MB/s — multiply MB/s by 1MB to get bytes
                totalRead = static_cast<quint64>(parts.at(2).toDouble() * 1024 * 1024);
                totalWrite = totalRead; // iostat doesn't split r/w by default
            }
        }
    } catch (...) {}

    diskReadWrite.append(totalRead);
    diskReadWrite.append(totalWrite);

    return diskReadWrite;
}

QStringList DiskInfo::getDiskNames() const
{
    QStringList diskNames;
    // On macOS, list disk devices from /dev/disk*
    QDir devDir("/dev");
    QStringList entries = devDir.entryList({"disk*"}, QDir::System);
    for (const QString &entry : entries) {
        // Only include base disks (disk0, disk1), not partitions (disk0s1)
        if (!entry.contains('s'))
            diskNames.append(entry);
    }
    return diskNames;
}

#endif
