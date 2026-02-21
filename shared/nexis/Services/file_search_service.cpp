#include "file_search_service.h"

#include <QFileInfo>
#include <QtConcurrent>

#include <Utils/command_util.h>

#ifdef Q_OS_MACOS
#include "file_search_tool_macos.h"
#else
#include "file_search_tool_linux.h"
#endif

FileSearchService *FileSearchService::instance = nullptr;

FileSearchService *FileSearchService::ins()
{
    if (!instance)
        instance = new FileSearchService;
    return instance;
}

FileSearchService::FileSearchService(QObject *parent)
    : QObject(parent)
{
#ifdef Q_OS_MACOS
    mTool = std::make_unique<FileSearchToolMacOS>();
#else
    mTool = std::make_unique<FileSearchToolLinux>();
#endif
}

void FileSearchService::search(const FileSearchParams &params)
{
    QStringList args = mTool->buildFindArgs(params);
    bool asRoot = params.searchAsRoot;

    (void)QtConcurrent::run([this, args, asRoot]() {
        bool hadError = false;
        QString result;

        try {
            if (asRoot) {
                result = CommandUtil::sudoExec("find", args);
            } else {
                result = CommandUtil::exec("find", args, {}, 120000);
            }
        } catch (QString) {
            hadError = true;
        }

        QStringList results;
        if (!hadError && !result.trimmed().isEmpty()) {
            results = result.split("\n");
        }

        emit searchFinished(results, hadError);
    });
}

void FileSearchService::moveToTrash(const QString &filePath, const QString &fileName, const QString &currentUser)
{
    bool isAnotherUser = QFileInfo(filePath).owner() != currentUser;
    QStringList args = mTool->buildMoveToTrashArgs(filePath, isAnotherUser);

    if (isAnotherUser) {
        CommandUtil::sudoExec("mv", args);
    } else {
        CommandUtil::exec("mv", args);
    }

    // Write trash metadata (Linux only; macOS no-ops)
    if (!QFile(filePath).exists()) {
        mTool->writeTrashMetadata(filePath, fileName);
    }
}

void FileSearchService::deleteFile(const QString &filePath, const QString &currentUser)
{
    bool isAnotherUser = QFileInfo(filePath).owner() != currentUser;
    if (isAnotherUser) {
        CommandUtil::sudoExec("rm", {"-rf", filePath});
    } else {
        CommandUtil::exec("rm", {"-rf", filePath});
    }
}

QString FileSearchService::trashPath() const
{
    return mTool->trashPath();
}
