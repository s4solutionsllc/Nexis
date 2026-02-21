#ifndef FILE_SEARCH_TOOL_H
#define FILE_SEARCH_TOOL_H

#include <QString>
#include <QStringList>

struct FileSearchParams {
    QString directory;
    QString namePattern;
    bool caseInsensitive = false;
    bool isRegex = false;
    bool invertMatch = false;
    bool findEmpty = false;
    QString fileType;          // "all", "f", "d", "l"
    QString timeType;          // "-1", "-amin", "-mmin", "-cmin"
    QString timeCriteria;      // "-", "", "+"
    int timeValue = 0;
    bool permReadable = false;
    bool permWritable = false;
    bool permExecutable = false;
    QString sizeCriteria;      // "-1", "-", "", "+"
    int sizeValue = 0;
    QString sizeUnit;          // "c", "k", "M", "G"
    QString userName;          // empty = no filter
    QString groupName;         // empty = no filter
    bool searchAsRoot = false;
};

class FileSearchTool
{
public:
    virtual ~FileSearchTool() = default;

    virtual QStringList buildFindArgs(const FileSearchParams &params) const = 0;
    virtual QString trashPath() const = 0;
    virtual QStringList buildMoveToTrashArgs(const QString &filePath, bool asRoot) const = 0;
    virtual void writeTrashMetadata(const QString &filePath, const QString &fileName) const;
};

#endif // FILE_SEARCH_TOOL_H
