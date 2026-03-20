#ifndef REPO_HEALTH_CHECKER_MACOS_H
#define REPO_HEALTH_CHECKER_MACOS_H

#include "Tools/repo_health_checker.h"
#include <Tools/package_tool_shared.h>

class RepoHealthCheckerMac : public RepoHealthChecker
{
public:
    RepoHealthCache checkAll(const QList<APTSourcePtr> &sources) override;
    RepoHealthResult checkOne(const APTSourcePtr &source) override;

    // Homebrew-specific: check all packages in batch
    RepoHealthCache checkBrewPackages(const QList<Package> &packages);

private:
    void checkOutdated(RepoHealthCache &cache);
    void checkDeprecated(RepoHealthCache &cache);
    void checkTaps(RepoHealthCache &cache);
};

#endif // REPO_HEALTH_CHECKER_MACOS_H
