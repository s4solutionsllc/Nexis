#include "repo_repair_engine.h"
#include "Utils/command_util.h"
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QDateTime>
#include <QRegularExpression>

RepoRepairEngine::RepairResult RepoRepairEngine::runCommand(const QString &command)
{
    QStringList args = command.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
    if (args.isEmpty())
        return {false, {}, QObject::tr("Empty command")};

    ExecResult r = CommandUtil::execWithStatus("pkexec", args, 60000);

    if (r.exitCode == 0)
        return {true, QObject::tr("Command completed successfully"), {}};
    if (r.exitCode == 126 || r.exitCode == 127)
        return {false, QObject::tr("Authentication cancelled"), {}};

    return {false, QObject::tr("Command failed (exit code %1)").arg(r.exitCode),
            r.error.isEmpty() ? r.output : r.error};
}

bool RepoRepairEngine::writeFileElevated(const QString &tempPath, const QString &destPath)
{
    ExecResult r = CommandUtil::execWithStatus("pkexec", {"cp", tempPath, destPath}, 30000);
    return r.exitCode == 0;
}

bool RepoRepairEngine::removeFileElevated(const QString &path)
{
    ExecResult r = CommandUtil::execWithStatus("pkexec", {"rm", path}, 30000);
    return r.exitCode == 0;
}

bool RepoRepairEngine::backupFile(const QString &filePath)
{
    QString backupDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/backups";
    QDir().mkpath(backupDir);

    QFileInfo fi(filePath);
    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss");
    QString backupPath = backupDir + "/" + fi.fileName() + "." + timestamp;

    return QFile::copy(filePath, backupPath);
}
