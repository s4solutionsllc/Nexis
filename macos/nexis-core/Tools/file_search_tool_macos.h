#ifndef FILE_SEARCH_TOOL_MACOS_H
#define FILE_SEARCH_TOOL_MACOS_H

#include <Tools/file_search_tool.h>

class FileSearchToolMacOS : public FileSearchTool
{
public:
    QStringList buildFindArgs(const FileSearchParams &params) const override;
    QString trashPath() const override;
    QStringList buildMoveToTrashArgs(const QString &filePath, bool asRoot) const override;
};

#endif // FILE_SEARCH_TOOL_MACOS_H
