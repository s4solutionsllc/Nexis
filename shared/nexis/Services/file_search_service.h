#ifndef FILE_SEARCH_SERVICE_H
#define FILE_SEARCH_SERVICE_H

#include <QObject>
#include <memory>

#include <Tools/file_search_tool.h>

class FileSearchService : public QObject
{
    Q_OBJECT

public:
    static FileSearchService *ins();

    void search(const FileSearchParams &params);
    void moveToTrash(const QString &filePath, const QString &fileName, const QString &currentUser);
    void deleteFile(const QString &filePath, const QString &currentUser);

    QString trashPath() const;

signals:
    void searchFinished(QStringList results, bool hadError);

private:
    explicit FileSearchService(QObject *parent = nullptr);
    static FileSearchService *instance;

    std::unique_ptr<FileSearchTool> mTool;
};

#endif // FILE_SEARCH_SERVICE_H
