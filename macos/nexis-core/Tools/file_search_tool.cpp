#include "file_search_tool_macos.h"
#include <QDir>

QStringList FileSearchToolMacOS::buildFindArgs(const FileSearchParams &params) const
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

    // Permissions — BSD find uses -perm +mode
    if (params.permReadable) {
        args.append("-perm");
        args.append("+r");
    }
    if (params.permWritable) {
        args.append("-perm");
        args.append("+w");
    }
    if (params.permExecutable) {
        args.append("-perm");
        args.append("+x");
    }

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

QString FileSearchToolMacOS::trashPath() const
{
    return QDir::homePath() + "/.Trash";
}

QStringList FileSearchToolMacOS::buildMoveToTrashArgs(const QString &filePath, bool) const
{
    return { filePath, trashPath() };
}
