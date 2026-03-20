#include "repo_health_checker.h"

QString RepoHealthChecker::cacheKey(const APTSourcePtr &source)
{
    return source->uri + " " + source->suites + " " + source->components;
}
