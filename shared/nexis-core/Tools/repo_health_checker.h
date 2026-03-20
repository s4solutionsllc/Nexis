#ifndef REPO_HEALTH_CHECKER_H
#define REPO_HEALTH_CHECKER_H

#include "repo_health_types.h"
#include "apt_source_tool.h"
#include <QList>

class RepoHealthChecker
{
public:
    virtual ~RepoHealthChecker() = default;

    // Run health checks on all repos. Blocking — call from background thread.
    virtual RepoHealthCache checkAll(const QList<APTSourcePtr> &sources) = 0;

    // Run health check on a single repo.
    virtual RepoHealthResult checkOne(const APTSourcePtr &source) = 0;

    // Composite cache key for a source entry
    static QString cacheKey(const APTSourcePtr &source);
};

#endif // REPO_HEALTH_CHECKER_H
