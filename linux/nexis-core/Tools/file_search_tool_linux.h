#ifndef FILE_SEARCH_TOOL_LINUX_H
#define FILE_SEARCH_TOOL_LINUX_H

#include <Tools/file_search_tool.h>

class FileSearchToolLinux : public FileSearchTool
{
public:
    QStringList buildFindArgs(const FileSearchParams &params) const override;
    QString trashPath() const override;
    QStringList buildMoveToTrashArgs(const QString &filePath, bool asRoot) const override;
    void writeTrashMetadata(const QString &filePath, const QString &fileName) const override;
};

#endif // FILE_SEARCH_TOOL_LINUX_H
