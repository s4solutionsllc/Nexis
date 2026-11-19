#include "process_tree_util.h"

#include <QSet>

QHash<pid_t, pid_t> ProcessTreeUtil::resolveParents(const QHash<pid_t, pid_t> &ppidByPid)
{
    QHash<pid_t, pid_t> parents;
    parents.reserve(ppidByPid.size());
    for (auto it = ppidByPid.constBegin(); it != ppidByPid.constEnd(); ++it) {
        const pid_t ppid = it.value();
        parents.insert(it.key(), (ppid != it.key() && ppidByPid.contains(ppid)) ? ppid : 0);
    }

    // A cycle (only possible from a torn snapshot) has no root to hang off,
    // so detach the lowest pid in it. Each chain is walked once.
    QSet<pid_t> settled;
    for (const pid_t start : ppidByPid.keys()) {
        QList<pid_t> chain;
        QSet<pid_t> onChain;
        pid_t cur = start;
        while (cur != 0 && !settled.contains(cur) && !onChain.contains(cur)) {
            onChain.insert(cur);
            chain.append(cur);
            cur = parents.value(cur);
        }
        if (cur != 0 && onChain.contains(cur)) {
            pid_t lowest = cur;
            for (pid_t p = parents.value(cur); p != cur; p = parents.value(p))
                lowest = qMin(lowest, p);
            parents[lowest] = 0;
        }
        for (pid_t p : chain)
            settled.insert(p);
    }
    return parents;
}
