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
        // SSO-3367 / audit A1: CommandUtil no longer throws. The previous
        // try/catch (SSO-3365 / audit H4) is redundant; failure is reported by
        // ExecResult::ok().
        const ExecResult result = asRoot
            ? CommandUtil::sudoExecWithStatus("find", args)
            : CommandUtil::execWithStatus("find", args, 120000);

        QStringList results;
        if (result.ok() && !result.output.isEmpty()) {
            results = result.output.split("\n");
        }

        emit searchFinished(results, !result.ok());
    });
}

void FileSearchService::moveToTrash(const QString &filePath, const QString &fileName, const QString &currentUser)
{
    bool isAnotherUser = QFileInfo(filePath).owner() != currentUser;
    QStringList args = mTool->buildMoveToTrashArgs(filePath, isAnotherUser);

    (void)QtConcurrent::run([this, filePath, fileName, args, isAnotherUser]() {
        // SSO-3367 / audit A1: unified ExecResult contract replaces the
        // SSO-3365 try/catch around CommandUtil::exec (which used to throw a
        // raw QString on QProcess failure).
        const ExecResult result = isAnotherUser
            ? CommandUtil::sudoExecWithStatus("mv", args)
            : CommandUtil::execWithStatus("mv", args, kFileOpTimeoutMs);

        const bool hadError = !result.ok();

        // Write trash metadata (Linux only; macOS no-ops). Skip on failure —
        // the file is still where it was. mTool is set in the ctor and never
        // reassigned, so reading it from the worker is safe.
        if (!hadError && !QFile(filePath).exists()) {
            mTool->writeTrashMetadata(filePath, fileName);
        }

        emit fileOperationFinished(FileOperation::MoveToTrash, filePath, hadError, result.error);
    });
}

void FileSearchService::deleteFile(const QString &filePath, const QString &currentUser)
{
    bool isAnotherUser = QFileInfo(filePath).owner() != currentUser;

    (void)QtConcurrent::run([this, filePath, isAnotherUser]() {
        // SSO-3367 / audit A1: see moveToTrash — same unified ExecResult path.
        const ExecResult result = isAnotherUser
            ? CommandUtil::sudoExecWithStatus("rm", {"-rf", filePath})
            : CommandUtil::execWithStatus("rm", {"-rf", filePath}, kFileOpTimeoutMs);

        emit fileOperationFinished(FileOperation::Delete, filePath, !result.ok(), result.error);
    });
}

QString FileSearchService::trashPath() const
{
    return mTool->trashPath();
}
