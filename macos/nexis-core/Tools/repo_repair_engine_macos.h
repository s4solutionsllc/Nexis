#ifndef REPO_REPAIR_ENGINE_MACOS_H
#define REPO_REPAIR_ENGINE_MACOS_H

#include "Tools/repo_repair_engine.h"

class RepoRepairEngineMac : public RepoRepairEngine
{
public:
    RepairResult convertToDeb822(const APTSourcePtr &) override { return {false, {}, {}}; }
    RepairResult removeDuplicate(const APTSourcePtr &) override { return {false, {}, {}}; }
    RepairResult disableSource(const APTSourcePtr &) override { return {false, {}, {}}; }
    RepairResult enableSource(const APTSourcePtr &) override { return {false, {}, {}}; }
    RepairResult removeSource(const APTSourcePtr &) override { return {false, {}, {}}; }
    void diagnoseConnection(const APTSourcePtr &) override {}
};

#endif // REPO_REPAIR_ENGINE_MACOS_H
