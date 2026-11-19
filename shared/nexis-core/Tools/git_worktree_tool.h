#ifndef GIT_WORKTREE_TOOL_H
#define GIT_WORKTREE_TOOL_H

#include <QDateTime>
#include <QList>
#include <QString>
#include <QStringList>

#include <functional>

#include "nexis-core_global.h"

// SSO-27100: finds git worktrees that AI coding agents (Claude Code, Codex,
// Cursor, Paperclip) leave behind and removes them with git's own commands.
//
// Three kinds of row are reported, and only these:
//   - MissingFolder: git still registers the worktree but its folder is gone.
//   - Unregistered:  the folder is still there, but its repository no longer
//                    registers it (the admin entry was pruned or the repo was
//                    deleted), so git cannot tell what the folder holds.
//   - Idle:          registered, present, in an agent work folder and untouched
//                    for `idleDays`. This is the only kind where dirty/unpushed
//                    status is knowable, so it is the only one that can be
//                    judged before removal.
//
// Removal never deletes files directly for a registered worktree: it is
// `git worktree remove` without --force, so git refuses a dirty tree itself.
// An Unregistered folder goes to the Trash and is recorded in the audit log
// (restorable from System Cleaner -> History). Removal is refused while a
// process has its working directory inside the folder, and in any scheduled or
// headless run.
namespace GitWorktreeTool {

struct NEXISCORESHARED_EXPORT Worktree {
    enum class Kind { MissingFolder, Unregistered, Idle };

    Kind kind = Kind::Unregistered;
    QString path;                // worktree folder as git or the .git file records it
    QString repoPath;            // main working tree; empty when it cannot be found
    QString branch;              // short branch name; empty when detached/unknown
    bool folderExists = false;
    int dirtyCount = -1;         // changed + untracked entries; -1 = unknown
    int unpushedCount = -1;      // commits on no remote; -1 = unknown or no remote
    quint64 sizeBytes = 0;       // 0 for MissingFolder
    QDateTime lastActivity;      // invalid for MissingFolder
    bool inUse = false;          // a process has its cwd inside the folder
};

// Returns a non-empty, translated reason when `wt` must not be removed, and an
// empty string when it may be offered for removal.
NEXISCORESHARED_EXPORT QString blockReason(const Worktree &wt);

struct NEXISCORESHARED_EXPORT ScanOptions {
    QStringList repoSearchRoots;  // each is searched for repos itself and one level down
    QStringList agentRoots;       // global agent work folders (e.g. ~/.codex/worktrees)
    int idleDays = 30;
    // True when a process has its cwd inside the path. Defaults to the
    // platform probe (isPathInUse) when unset.
    std::function<bool(const QString &)> inUse;

    // Home-relative defaults for the current user.
    static ScanOptions forHome(const QString &home);
};

// True when `path` lies inside one of the known agent work folders
// (.claude/.codex/.cursor/.paperclip + "/worktrees/").
NEXISCORESHARED_EXPORT bool isAgentWorktreePath(const QString &path);

// False when git cannot be run (not installed; on macOS also when the Xcode
// command line tools are missing, so /usr/bin/git would only raise the
// installer dialog).
NEXISCORESHARED_EXPORT bool gitAvailable();

// Walks the repos and agent work folders and returns every row, sorted by
// path. Read-only. Slow on large trees: call off the UI thread.
NEXISCORESHARED_EXPORT QList<Worktree> scan(const ScanOptions &options);

struct NEXISCORESHARED_EXPORT RemoveOptions {
    // Arguments of the running Nexis process; a --clean/--report style run is
    // refused. Defaults to QCoreApplication::arguments() when unset.
    QStringList appArguments;
    bool appArgumentsSet = false;
    std::function<bool(const QString &)> inUse;  // as in ScanOptions
    // Folder the deny-list confines removal to. Defaults to the real home;
    // tests set it because macOS does not resolve a redirected $HOME.
    QString homeRoot;
    // Defaults to QFile::moveToTrash. Tests substitute a fake Trash.
    std::function<bool(const QString &path, QString *trashDestination)> moveToTrash;
};

struct NEXISCORESHARED_EXPORT RemoveResult {
    bool ok = false;
    QString message;  // user-facing, translated
};

// Re-inspects `wt` and removes it. Every precondition is checked again here,
// not trusted from the scan: headless run, deny-list, process inside, dirty,
// still registered / still unregistered.
NEXISCORESHARED_EXPORT RemoveResult removeWorktree(const Worktree &wt, const RemoveOptions &options = {});

// True when a process other than this one has its working directory at or
// below `path`. Fails closed (returns true) when that cannot be determined.
NEXISCORESHARED_EXPORT bool isPathInUse(const QString &path);

// Exposed for tests.
NEXISCORESHARED_EXPORT bool isPathInUseProc(const QString &procRoot, const QString &path, qint64 ownPid);
NEXISCORESHARED_EXPORT QStringList parseLsofCwdPaths(const QString &lsofFieldOutput);

} // namespace GitWorktreeTool

#endif // GIT_WORKTREE_TOOL_H
