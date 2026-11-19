#ifndef DEV_ARTIFACT_SCANNER_H
#define DEV_ARTIFACT_SCANNER_H

#include <QAtomicInt>
#include <QDateTime>
#include <QList>
#include <QString>
#include <QStringList>

#include <functional>

#include "nexis-core_global.h"

// SSO-27098: stale developer-project artifact cleaner. Finds projects by marker
// file (package.json, Cargo.toml, ...) under user-chosen roots and offers each
// project's rebuildable folders (node_modules, .venv, target, build, Pods,
// .gradle) as ONE row per project, aged by the project's real activity.
//
// Safety model, in the order it is applied:
//  - Allowlist only: a folder is offered only when a marker for its project
//    type sits next to it AND the folder is a real directory (never a symlink)
//    whose canonical path stays inside the project root.
//  - $HOME itself (and any directory that contains it), "/" and every
//    LifecycleDenyList::isSafeForCleaning() location are refused, both as scan
//    roots and as project roots, after symlinks are resolved.
//  - A project is never offered while a process has its cwd, executable or an
//    open file inside it, or while a git operation is in progress. If the
//    process table cannot be read the scan offers nothing (fail closed).
//  - Age is the newest file change anywhere in the project plus the git
//    index/HEAD/reflog — never the artifact folder's own mtime, which an
//    install or build refreshes.
//  - Deletion moves to the Trash, only for a human-driven (Interactive) run;
//    scheduled and headless contexts are refused outright.
namespace DevArtifacts {

enum class RunContext { Interactive, Scheduled, Headless };

struct NEXISCORESHARED_EXPORT Artifact {
    QString path;
    quint64 bytes = 0;
};

struct NEXISCORESHARED_EXPORT Project {
    QString root;                 // canonical project root
    QString name;                 // leaf directory name
    QStringList kinds;            // display names of the matched project types
    QList<Artifact> artifacts;
    QDateTime lastActivity;       // see lastActivity()

    quint64 totalBytes() const;
    qint64 idleDays(const QDateTime &now = QDateTime::currentDateTime()) const;
};

// Answers "does any process hold something inside this directory?" against a
// snapshot taken once per scan. available() is false when the snapshot could
// not be taken (no /proc, lsof missing or timed out); callers must then treat
// every project as in use.
class NEXISCORESHARED_EXPORT UseProbe
{
public:
    virtual ~UseProbe() = default;
    virtual bool available() const = 0;
    virtual bool holdsPathUnder(const QString &canonicalDir) const = 0;
};

class NEXISCORESHARED_EXPORT HeldPathsProbe : public UseProbe
{
public:
    // `heldPaths` are absolute, symlink-resolved paths (cwds, executables and
    // open files of running processes).
    HeldPathsProbe(const QStringList &heldPaths, bool available);

    // Linux: /proc/<pid>/{cwd,exe,fd/*}. macOS: one `lsof -nP -Fn` pass.
    // Other platforms: unavailable.
    static HeldPathsProbe fromSystem();

    bool available() const override { return mAvailable; }
    bool holdsPathUnder(const QString &canonicalDir) const override;

private:
    QStringList mHeld;            // sorted
    bool mAvailable;
};

struct NEXISCORESHARED_EXPORT ScanOptions {
    QStringList roots;
    QString homeOverride;         // tests; empty = the real home folder
    int maxDepth = 6;
    // Safety valves; a project or tree that exceeds them is skipped, not guessed at.
    int maxActivityEntries = 200000;
    int maxDirectories = 400000;
    // Seam for the user's "Always exclude" list; returns true to skip a path.
    std::function<bool(const QString &)> isExcluded;
};

struct NEXISCORESHARED_EXPORT ScanResult {
    QList<Project> projects;      // largest first
    QStringList refusedRoots;     // "<path>: <reason>"
    int skippedInUse = 0;
    int skippedGitOperation = 0;
    int skippedExcluded = 0;
    int skippedUnverifiable = 0;  // activity walk exceeded its cap
    bool probeUnavailable = false;
    bool cancelled = false;
};

// Why `path` may not be a scan root or project root; empty when acceptable.
NEXISCORESHARED_EXPORT QString rootRefusalReason(const QString &path,
                                                 const QString &homeOverride = QString());

// True while a merge/rebase/cherry-pick/revert/bisect is under way or a git
// lock file is present in the repository containing `projectRoot`.
NEXISCORESHARED_EXPORT bool gitOperationInProgress(const QString &projectRoot);

// Newest change in the project: file mtimes anywhere under `projectRoot`
// (skipping `.git`, any node_modules and `ignoredDirs`, never following
// symlinks), plus the repository's index, HEAD, ORIG_HEAD and reflog.
// Invalid QDateTime when the walk exceeded `maxEntries`.
NEXISCORESHARED_EXPORT QDateTime lastActivity(const QString &projectRoot,
                                              const QStringList &ignoredDirs = QStringList(),
                                              int maxEntries = 200000);

NEXISCORESHARED_EXPORT ScanResult scan(const ScanOptions &options,
                                       const UseProbe &probe,
                                       const QAtomicInt *cancel = nullptr);

struct NEXISCORESHARED_EXPORT TrashRequest {
    QString projectRoot;
    QString artifactPath;
    QDateTime scannedActivity;    // Project::lastActivity at scan time
};

struct NEXISCORESHARED_EXPORT TrashOutcome {
    TrashRequest request;
    bool trashed = false;
    QString reason;               // why it was refused or failed; empty on success
};

// Re-validates every request against the live filesystem and process table
// (the listing may be minutes old) and moves what still qualifies to the
// Trash through `moveToTrash`. A project whose activity is newer than the
// scan saw is left alone. Non-Interactive contexts refuse every request.
NEXISCORESHARED_EXPORT QList<TrashOutcome> trashArtifacts(
    const QList<TrashRequest> &requests,
    RunContext context,
    const UseProbe &probe,
    const std::function<bool(const QString &)> &moveToTrash,
    const std::function<bool(const QString &)> &isExcluded = {},
    const QString &homeOverride = QString());

} // namespace DevArtifacts

#endif // DEV_ARTIFACT_SCANNER_H
