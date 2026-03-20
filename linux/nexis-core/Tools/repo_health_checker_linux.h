#ifndef REPO_HEALTH_CHECKER_LINUX_H
#define REPO_HEALTH_CHECKER_LINUX_H

#include "Tools/repo_health_checker.h"

class RepoHealthCheckerLinux : public RepoHealthChecker
{
public:
    RepoHealthCache checkAll(const QList<APTSourcePtr> &sources) override;
    RepoHealthResult checkOne(const APTSourcePtr &source) override;

private:
    // Individual checks — each appends issues to the result
    void checkConnection(const APTSourcePtr &source, RepoHealthResult &result);
    void checkReleaseFile(const APTSourcePtr &source, const QString &releaseContent, RepoHealthResult &result);
    void checkGpgKey(const APTSourcePtr &source, RepoHealthResult &result);
    void checkSuiteMismatch(const APTSourcePtr &source, RepoHealthResult &result);
    void checkDeprecatedFormat(const APTSourcePtr &source, RepoHealthResult &result);

    // Check duplicates across the full list (called from checkAll)
    void checkDuplicates(const QList<APTSourcePtr> &sources, RepoHealthCache &cache);

    // Resolve name/description via knowledge base + Release file fallback
    void resolveDescription(const APTSourcePtr &source, const QString &releaseContent, RepoHealthResult &result);

    // Fetch the InRelease or Release file contents (fetched once per repo, passed to multiple checks)
    QString fetchReleaseFile(const QString &uri, const QString &suite);

    // Compute overall status from issue list
    static RepoHealthResult::Status worstStatus(const QList<RepoHealthIssue> &issues);
};

#endif // REPO_HEALTH_CHECKER_LINUX_H
