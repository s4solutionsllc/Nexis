#ifndef REPO_REPAIR_ENGINE_H
#define REPO_REPAIR_ENGINE_H

#include <QObject>
#include <QString>
#include <QList>
#include <Tools/repo_health_types.h>
#include <Tools/apt_source_tool.h>

struct DiagnoseStep {
    enum Status { Ok, Warning, Failed };
    QString check;
    Status status = Ok;
    QString detail;
};

struct DiagnoseResult {
    QList<DiagnoseStep> steps;
    QString suggestion;
    QList<RepoRepairAction> followUpActions;
};

Q_DECLARE_METATYPE(DiagnoseStep)
Q_DECLARE_METATYPE(DiagnoseResult)

class RepoRepairEngine : public QObject
{
    Q_OBJECT
public:
    struct RepairResult {
        bool success;
        QString message;
        QString errorDetail;
    };

    virtual ~RepoRepairEngine() = default;

    // Shared (non-virtual)
    RepairResult runCommand(const QString &command);

    // Platform-specific (pure virtual)
    virtual RepairResult convertToDeb822(const APTSourcePtr &source) = 0;
    virtual RepairResult removeDuplicate(const APTSourcePtr &source) = 0;
    virtual RepairResult disableSource(const APTSourcePtr &source) = 0;
    virtual RepairResult enableSource(const APTSourcePtr &source) = 0;
    virtual RepairResult removeSource(const APTSourcePtr &source) = 0;
    virtual void diagnoseConnection(const APTSourcePtr &source) = 0;

    // Configurable output directory — tests override to use temp dirs
    void setSourcesDir(const QString &dir) { mSourcesDir = dir; }
    QString sourcesDir() const { return mSourcesDir; }

protected:
    // File helpers — virtual so tests can mock pkexec
    virtual bool writeFileElevated(const QString &tempPath, const QString &destPath);
    virtual bool removeFileElevated(const QString &path);
    bool backupFile(const QString &filePath);

    QString mSourcesDir = "/etc/apt/sources.list.d/";

signals:
    void diagnoseFinished(const DiagnoseResult &result);
};

#endif // REPO_REPAIR_ENGINE_H
