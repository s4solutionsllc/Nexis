#ifndef CLEANERML_PLATFORM_H
#define CLEANERML_PLATFORM_H

#include "cleanerml_model.h"

// SSO-25782: reduces a parsed CleanerML cleaner to what can actually run on
// one platform, so the UI never offers an option that would silently do
// nothing or only part of its job.
namespace CleanerML {

// "linux" or "macos".
NEXISCORESHARED_EXPORT QString currentOs();

// An empty list matches every platform. "unix" covers Linux and macOS;
// "darwin" is an alias for "macos".
NEXISCORESHARED_EXPORT bool osMatches(const QStringList &os, const QString &current);

// True when `path` can only name a location on another platform or needs a
// variable this cleaner does not define for `os`: a Windows path
// (backslashes, %VAR%), or a $$name$$ token other than home/cache with no
// value left in `vars`.
NEXISCORESHARED_EXPORT bool pathIsForeign(const QString &path, const QHash<QString, QList<VarValue>> &vars);

// Returns a copy holding only what applies to `os`:
//  - var values and running checks for other platforms are removed;
//  - actions for other platforms, or whose path is foreign, are removed;
//  - an option is dropped when it targets another platform, has nothing
//    left to do, or still contains an action Nexis cannot run
//    (ActionType::Unsupported, or a whole-home search="deep") — running the
//    rest would do only part of what the option's label promises.
// A cleaner for another platform comes back with no options.
NEXISCORESHARED_EXPORT Cleaner forPlatform(const Cleaner &cleaner, const QString &os);

} // namespace CleanerML

#endif // CLEANERML_PLATFORM_H
