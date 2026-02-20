#include "disk_info_linux.h"
#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>

#include "Utils/file_util.h"

QList<quint64> DiskInfoLinux::getDiskIO() const
{
    static QStringList diskNames = getDiskNames();

    QList<quint64> diskReadWrite;
    quint64 totalRead = 0;
    quint64 totalWrite = 0;

    for (const QString &diskName : diskNames) {
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

QStringList DiskInfoLinux::getDiskNames() const
{
    QDir blocks("/sys/block");
    QStringList disks;
    for (const QFileInfo &entryInfo : blocks.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot)) {
        if (QFile::exists(QString("%1/device").arg(entryInfo.absoluteFilePath()))) {
            disks.append(entryInfo.baseName());
        }
    }
    return disks;
}
