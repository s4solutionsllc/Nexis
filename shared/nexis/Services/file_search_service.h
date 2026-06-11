#ifndef FILE_SEARCH_SERVICE_H
#define FILE_SEARCH_SERVICE_H

#include <QObject>
#include <memory>

#include <Tools/file_search_tool.h>

class FileSearchService : public QObject
{
    Q_OBJECT

public:
    enum class FileOperation { MoveToTrash, Delete };
    Q_ENUM(FileOperation)

    static FileSearchService *ins();

    void search(const FileSearchParams &params);

    // SSO-3365 / audit H4: dispatch on a worker thread; emit fileOperationFinished
    // when done. CommandUtil::exec throws QString on QProcess errors (including
    // the 30 s timeout); these calls catch and report via the signal instead of
    // letting the exception escape through the UI event loop and abort the app.
    void moveToTrash(const QString &filePath, const QString &fileName, const QString &currentUser);
    void deleteFile(const QString &filePath, const QString &currentUser);

    QString trashPath() const;

signals:
    void searchFinished(QStringList results, bool hadError);
    void fileOperationFinished(FileSearchService::FileOperation op,
                               QString filePath,
                               bool hadError,
                               QString errorMessage);

private:
    explicit FileSearchService(QObject *parent = nullptr);
    static FileSearchService *instance;

    std::unique_ptr<FileSearchTool> mTool;
};

Q_DECLARE_METATYPE(FileSearchService::FileOperation)

#endif // FILE_SEARCH_SERVICE_H
