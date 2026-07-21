#ifndef RUNNING_APP_GATE_H
#define RUNNING_APP_GATE_H

#include <QStringList>
#include <functional>

#include "nexis-core_global.h"

// SSO-15566 / SSO-15373 CISO §4: pure decision logic for the pre-uninstall
// running-process gate, kept free of QWidget/QDialog so it is unit-testable
// without a display. UninstallerPage supplies isRunning/promptToQuit backed
// by PackageService::isAppRunning() and RunningAppWarningDialog; tests
// supply fakes.
namespace RunningAppGate {

// isRunning(path) — true when the app at `path` currently has a running
// process (PackageTool::isAppRunning).
// promptToQuit(path) — shown only when isRunning(path) is true. Returns true
// iff the app is confirmed no longer running and this item should proceed to
// deletion; false means the user cancelled and this item alone is skipped —
// it must not affect the rest of the batch.
// Returns the subset of `paths`, in order, cleared to proceed to trashApps().
NEXISCORESHARED_EXPORT QStringList filterRunnable(
    const QStringList &paths,
    const std::function<bool(const QString &)> &isRunning,
    const std::function<bool(const QString &)> &promptToQuit);

} // namespace RunningAppGate

#endif // RUNNING_APP_GATE_H
