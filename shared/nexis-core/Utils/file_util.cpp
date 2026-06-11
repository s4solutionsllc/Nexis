#include "file_util.h"

#include "command_util.h"

FileUtil::FileUtil()
{

}

QString FileUtil::readStringFromFile(const QString &path, const QIODevice::OpenMode &mode)
{
    QSharedPointer<QFile> file(new QFile(path));

    QString data;

    if(file->open(mode)) {

      data = file->readAll();

      file->close();
    }

    return data;
}

QStringList FileUtil::readListFromFile(const QString &path, const QIODevice::OpenMode &mode)
{
    QStringList list = FileUtil::readStringFromFile(path, mode).trimmed().split("\n");

    return list;
}

bool FileUtil::writeFile(const QString &path, const QString &content, const QIODevice::OpenMode &mode)
{
    QFile file(path);

    if(file.open(mode))
    {
        QTextStream stream(&file);
        stream << content.toUtf8() << Qt::endl;

        file.close();

        return true;
    }

    return false;
}

QStringList FileUtil::directoryList(const QString &path)
{
    QDir dir(path);

    QStringList list;

    for (const QFileInfo &info : dir.entryInfoList(QDir::NoDotAndDotDot | QDir::Files))
        list << info.fileName();

    return list;
}

bool FileUtil::writeRootFile(const QString &path, const QByteArray &content)
{
#ifdef Q_OS_LINUX
    // Pipe `content` through pkexec tee <path>. Post SSO-3367, sudoExec
    // surfaces real exit status via sudoExecWithStatus — pkexec returns 0 only
    // if both the auth and the wrapped `tee` succeeded, so the read-back byte
    // compare the pre-SSO-3367 version did is no longer needed.
    return CommandUtil::sudoExecWithStatus("tee", {path}, content).ok();
#else
    Q_UNUSED(path)
    Q_UNUSED(content)
    return false;   // macOS/other platforms have no pkexec — caller should
                    // gate calls to writeRootFile() on platform anyway.
#endif
}

quint64 FileUtil::getFileSize(const QString &path)
{
    quint64 totalSize = 0;

    QFileInfo info(path);

    if (info.exists())
    {
        if (info.isFile()) {
            totalSize += info.size();
        }
        else if (info.isDir()) {

            QDir dir(path);

            for (const QFileInfo &i : dir.entryInfoList(QDir::NoDotAndDotDot | QDir::Files | QDir::Dirs)) {
                totalSize += getFileSize(i.absoluteFilePath());
            }
        }
    }

    return totalSize;
}


