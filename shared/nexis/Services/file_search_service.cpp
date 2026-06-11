#include "file_search_service.h"

#include <QFileInfo>
#include <QtConcurrent>

#include <Utils/command_util.h>

#ifdef Q_OS_MACOS
#include "file_search_tool_macos.h"
#else
#include "file_search_tool_linux.h"
#endif

namespace {
// SSO-3365: bulk deletes can take minutes. The default 30 s exec timeout
// killed `mv`/`rm` mid-op and surfaced as a thrown QString that aborted the
// app. Give file operations five minutes before we give up.
constexpr int kFileOpTimeoutMs = 5 * 60 * 1000;
} // namespace

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
    // Required for fileOperationFinished to marshal across the worker→UI
    // queued connection.
    qRegisterMetaType<FileSearchService::FileOperation>("FileSearchService::FileOperation");

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

    (void)QtConcurrent::run([this, filePath, fileName, args, isAnotherUser]() {
        bool hadError = false;
        QString errorMessage;

        try {
            if (isAnotherUser) {
                CommandUtil::sudoExec("mv", args);
            } else {
                CommandUtil::exec("mv", args, {}, kFileOpTimeoutMs);
            }
        } catch (const QString &ex) {
            hadError = true;
            errorMessage = ex;
        }

        // Write trash metadata (Linux only; macOS no-ops). Skip on failure —
        // the file is still where it was. mTool is set in the ctor and never
        // reassigned, so reading it from the worker is safe.
        if (!hadError && !QFile(filePath).exists()) {
            mTool->writeTrashMetadata(filePath, fileName);
        }

        emit fileOperationFinished(FileOperation::MoveToTrash, filePath, hadError, errorMessage);
    });
}

void FileSearchService::deleteFile(const QString &filePath, const QString &currentUser)
{
    bool isAnotherUser = QFileInfo(filePath).owner() != currentUser;

    (void)QtConcurrent::run([this, filePath, isAnotherUser]() {
        bool hadError = false;
        QString errorMessage;

        try {
            if (isAnotherUser) {
                CommandUtil::sudoExec("rm", {"-rf", filePath});
            } else {
                CommandUtil::exec("rm", {"-rf", filePath}, {}, kFileOpTimeoutMs);
            }
        } catch (const QString &ex) {
            hadError = true;
            errorMessage = ex;
        }

        emit fileOperationFinished(FileOperation::Delete, filePath, hadError, errorMessage);
    });
}

QString FileSearchService::trashPath() const
{
    return mTool->trashPath();
}
