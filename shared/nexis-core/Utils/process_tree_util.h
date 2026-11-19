#ifndef PROCESS_TREE_UTIL_H
#define PROCESS_TREE_UTIL_H

#include "nexis-core_global.h"

#include <QHash>

#include <sys/types.h>

// SSO-27153: parent/child resolution for the Processes page's tree mode.
// A raw ppid can't be used as the tree parent directly — the parent may be
// outside the visible set (filtered by user, exited, or unreadable), or a
// snapshot race can produce a self-parent or a pid-reuse cycle. Those
// processes are promoted to roots so every process stays reachable.
class NEXISCORESHARED_EXPORT ProcessTreeUtil
{
public:
    // Maps each pid to the pid it should nest under, or 0 for a root.
    static QHash<pid_t, pid_t> resolveParents(const QHash<pid_t, pid_t> &ppidByPid);
};

#endif // PROCESS_TREE_UTIL_H
