#include "file_search_tool_linux.h"
#include <QDir>
#include <QDateTime>
#include <Utils/file_util.h>

QStringList FileSearchToolLinux::buildFindArgs(const FileSearchParams &params) const
{
    QStringList args;
    args.append(params.directory);

    if (!params.namePattern.isEmpty()) {
        if (params.caseInsensitive) {
            args.append(params.isRegex ? "-iregex" : "-iname");
        } else {
            args.append(params.isRegex ? "-regex" : "-name");
        }
        args.append(params.namePattern);
    }

    if (params.invertMatch)
        args.append("-invert");

    if (params.findEmpty)
        args.append("-empty");

    if (params.fileType != "all") {
        args.append("-type");
        args.append(params.fileType);
    }

    // Time filter
    if (params.timeType != "-1") {
        args.append(params.timeType);
        args.append(QString("%1%2").arg(params.timeCriteria).arg(params.timeValue));
    }

    // Permissions — GNU find uses -readable/-writable/-executable
    if (params.permReadable)
        args.append("-readable");
    if (params.permWritable)
        args.append("-writable");
    if (params.permExecutable)
        args.append("-executable");

    // Size filter
    if (params.sizeCriteria != "-1") {
        QString size = QString("%1%2%3")
                .arg(params.sizeCriteria)
                .arg(params.sizeValue)
                .arg(params.sizeUnit);
        args.append("-size");
        args.append(size);
    }

    // Owner filter
    if (!params.userName.isEmpty()) {
        args.append("-user");
        args.append(params.userName);
    }
    if (!params.groupName.isEmpty()) {
        args.append("-group");
        args.append(params.groupName);
    }

    return args;
}

QString FileSearchToolLinux::trashPath() const
{
    return QDir::homePath() + "/.local/share/Trash";
}

QStringList FileSearchToolLinux::buildMoveToTrashArgs(const QString &filePath, bool) const
{
    return { filePath, trashPath() + "/files" };
}

void FileSearchToolLinux::writeTrashMetadata(const QString &filePath, const QString &fileName) const
{
    QString infoContent = QString("[Trash Info]\n"
                        "Path=%1\n"
                        "DeletionDate=%2")
            .arg(filePath)
            .arg(QDateTime::currentDateTime().toString("yyyy-MM-ddThh:mm:ss"));

    FileUtil::writeFile(trashPath() + "/info/" + fileName + ".trashinfo", infoContent);
}
