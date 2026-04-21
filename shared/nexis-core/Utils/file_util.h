#ifndef FILEUTIL_H
#define FILEUTIL_H

#include <QFile>
#include <QDir>
#include <QTextStream>
#include <QDirIterator>
#include <QStandardPaths>

#include <QStandardPaths>
#include <QSharedPointer>

#include "nexis-core_global.h"

class NEXISCORESHARED_EXPORT FileUtil
{
public:
  static QString readStringFromFile(const QString &path, const QIODevice::OpenMode &mode = QIODevice::ReadOnly);
  static QStringList readListFromFile(const QString &path, const QIODevice::OpenMode &mode = QIODevice::ReadOnly);

  static bool writeFile(const QString &path, const QString &content, const QIODevice::OpenMode &mode = QIODevice::WriteOnly | QIODevice::Truncate);
  static QStringList directoryList(const QString &path);
  static quint64 getFileSize(const QString &path);

  // FR-81 / FR-117: write a root-owned file by piping `content` through
  // `tee <path>` via pkexec/sudoExec. On Linux only — the macOS build is
  // a compile-time no-op returning false. Re-reads the file after the
  // write and returns true iff the on-disk bytes match `content` exactly.
  //
  // `path` must be absolute. Typical uses: /etc/sysctl.d/*.conf,
  // /etc/systemd/system/*.service.
  static bool writeRootFile(const QString &path, const QByteArray &content);

private:
  FileUtil();
};

#endif // FILEUTIL_H
