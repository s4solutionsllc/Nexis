#ifndef REPO_REPAIR_ENGINE_LINUX_H
#define REPO_REPAIR_ENGINE_LINUX_H

#include "Tools/repo_repair_engine.h"
#include <functional>

class RepoRepairEngineLinux : public RepoRepairEngine
{
public:
    RepairResult convertToDeb822(const APTSourcePtr &source) override;
    RepairResult removeDuplicate(const APTSourcePtr &source) override;
    RepairResult disableSource(const APTSourcePtr &source) override;
    RepairResult enableSource(const APTSourcePtr &source) override;
    RepairResult removeSource(const APTSourcePtr &source) override;
    void diagnoseConnection(const APTSourcePtr &source) override;

private:
    RepairResult modifySourceFile(const APTSourcePtr &source,
        std::function<QString(const QString &line)> lineTransform,
        const QString &successMsg);

    QString buildMatchPattern(const APTSourcePtr &source) const;
};

#endif // REPO_REPAIR_ENGINE_LINUX_H
