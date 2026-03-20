#ifndef REPO_KNOWLEDGE_BASE_H
#define REPO_KNOWLEDGE_BASE_H

#include "repo_health_types.h"

class RepoKnowledgeBase
{
public:
    static RepoKnownInfo lookup(const QString &uri);
    static QString domainFromUri(const QString &uri);
};

#endif // REPO_KNOWLEDGE_BASE_H
