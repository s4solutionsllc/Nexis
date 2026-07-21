#include "running_app_gate.h"

namespace RunningAppGate {

QStringList filterRunnable(const QStringList &paths,
                           const std::function<bool(const QString &)> &isRunning,
                           const std::function<bool(const QString &)> &promptToQuit)
{
    QStringList runnable;
    for (const QString &path : paths) {
        if (!isRunning(path) || promptToQuit(path))
            runnable << path;
    }
    return runnable;
}

} // namespace RunningAppGate
