#include "git_worktree_tool.h"

#include "lifecycle_audit_log.h"
#include "lifecycle_deny_list.h"

#include <Utils/command_util.h>
#include <Utils/headless_util.h>

#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QSet>
#include <QUuid>

#include <algorithm>

namespace GitWorktreeTool {

namespace {

QString tr(const char *text)
{
    return QCoreApplication::translate("GitWorktreeTool", text);
}

constexpr int kGitTimeoutMs = 60000;
constexpr int kAgentRootDepth = 3;

const QStringList &agentFolderNames()
{
    static const QStringList names = {
        QStringLiteral(".claude"), QStringLiteral(".codex"),
        QStringLiteral(".cursor"), QStringLiteral(".paperclip"),
    };
    return names;
}

// core.fsmonitor is a repo-controlled command hook; --no-optional-locks keeps
// the dirty check from rewriting the index and so from faking "recent activity".
ExecResult runGit(const QString &dir, const QStringList &args)
{
    QStringList full = {QStringLiteral("--no-optional-locks"), QStringLiteral("-c"),
                        QStringLiteral("core.fsmonitor=false"), QStringLiteral("-C"), dir};
    full += args;
    return CommandUtil::execWithStatus(QStringLiteral("git"), full, kGitTimeoutMs);
}

QString firstLine(const QString &text)
{
    return text.section(QLatin1Char('\n'), 0, 0).trimmed();
}

bool isRepo(const QString &dir)
{
    return QFileInfo(dir + QStringLiteral("/.git")).isDir();
}

// What a folder's `.git` file says about it.
struct LinkedFolder {
    bool linked = false;      // `.git` is a file pointing at <common>/worktrees/<name>
    bool registered = false;  // that admin entry still exists
    QString admin;
    QString repoPath;
};

QString resolveAgainst(const QString &base, const QString &p)
{
    return QDir::cleanPath(QDir::isAbsolutePath(p) ? p : base + QLatin1Char('/') + p);
}

LinkedFolder inspectFolder(const QString &folder)
{
    LinkedFolder out;
    const QFileInfo dotGit(folder + QStringLiteral("/.git"));
    if (!dotGit.isFile())
        return out;

    QFile f(dotGit.filePath());
    if (!f.open(QIODevice::ReadOnly))
        return out;
    const QString line = QString::fromUtf8(f.readLine(4096)).trimmed();
    static const QString prefix = QStringLiteral("gitdir:");
    if (!line.startsWith(prefix))
        return out;

    const QString admin = resolveAgainst(folder, line.mid(prefix.size()).trimmed());
    const QFileInfo adminInfo(admin);
    // Submodules also use a `.git` file; only <common>/worktrees/<name> is a worktree.
    if (adminInfo.dir().dirName() != QLatin1String("worktrees"))
        return out;

    out.linked = true;
    out.admin = admin;
    out.registered = adminInfo.isDir();

    QString common = adminInfo.dir().absolutePath();
    common = QDir::cleanPath(common + QStringLiteral("/.."));
    const QString repo = QFileInfo(common).fileName() == QLatin1String(".git")
        ? QFileInfo(common).dir().absolutePath() : common;
    if (QFileInfo(common).isDir())
        out.repoPath = repo;
    return out;
}

QStringList findLinkedFolders(const QString &root, int depth)
{
    QStringList found;
    if (depth < 0 || !QFileInfo(root).isDir())
        return found;
    if (QFileInfo(root + QStringLiteral("/.git")).isFile()) {
        found << root;
        return found;
    }
    const QDir dir(root);
    const QStringList children = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks);
    for (const QString &child : children)
        found += findLinkedFolders(dir.filePath(child), depth - 1);
    return found;
}

quint64 folderSize(const QString &path)
{
    quint64 total = 0;
    QDirIterator it(path, QDir::Files | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        const QFileInfo fi = it.fileInfo();
        if (!fi.isSymLink())
            total += static_cast<quint64>(fi.size());
    }
    return total;
}

struct ListedWorktree {
    QString path;
    QString branch;
    bool locked = false;
};

// Linked worktrees of `repo` (the main one, always listed first, is dropped).
// Returns false when git could not list them.
bool listWorktrees(const QString &repo, QList<ListedWorktree> *out)
{
    const ExecResult r = runGit(repo, {QStringLiteral("worktree"), QStringLiteral("list"),
                                       QStringLiteral("--porcelain")});
    if (!r.ok())
        return false;
    const QStringList blocks = r.output.split(QStringLiteral("\n\n"), Qt::SkipEmptyParts);
    for (int i = 1; i < blocks.size(); ++i) {
        ListedWorktree wt;
        for (const QString &line : blocks.at(i).split(QLatin1Char('\n'))) {
            if (line.startsWith(QLatin1String("worktree ")))
                wt.path = QDir::cleanPath(line.mid(9));
            else if (line.startsWith(QLatin1String("branch refs/heads/")))
                wt.branch = line.mid(18);
            else if (line == QLatin1String("locked") || line.startsWith(QLatin1String("locked ")))
                wt.locked = true;
        }
        if (!wt.path.isEmpty())
            out->append(wt);
    }
    return true;
}

int dirtyCountOf(const QString &folder)
{
    const ExecResult r = runGit(folder, {QStringLiteral("status"), QStringLiteral("--porcelain")});
    if (!r.ok())
        return -1;
    return r.output.split(QLatin1Char('\n'), Qt::SkipEmptyParts).size();
}

int unpushedCountOf(const QString &folder)
{
    const ExecResult remotes = runGit(folder, {QStringLiteral("remote")});
    if (!remotes.ok() || remotes.output.trimmed().isEmpty())
        return -1;
    const ExecResult r = runGit(folder, {QStringLiteral("rev-list"), QStringLiteral("--count"),
                                         QStringLiteral("HEAD"), QStringLiteral("--not"),
                                         QStringLiteral("--remotes")});
    bool ok = false;
    const int n = r.ok() ? r.output.trimmed().toInt(&ok) : 0;
    return ok ? n : -1;
}

QDateTime lastActivityOf(const QString &folder, const QString &admin)
{
    QDateTime latest = QFileInfo(folder).lastModified();
    for (const QString &rel : {QStringLiteral("/HEAD"), QStringLiteral("/logs/HEAD")}) {
        const QFileInfo fi(admin + rel);
        if (fi.exists() && fi.lastModified() > latest)
            latest = fi.lastModified();
    }
    return latest;
}

bool probe(const std::function<bool(const QString &)> &inUse, const QString &path)
{
    return inUse ? inUse(path) : isPathInUse(path);
}

QStringList defaultRepoSearchRoots(const QString &home)
{
    QStringList roots = {home};
    for (const QString &rel : {QStringLiteral("Documents"), QStringLiteral("Documents/GitHub"),
                               QStringLiteral("Projects"), QStringLiteral("projects"),
                               QStringLiteral("dev"), QStringLiteral("src"),
                               QStringLiteral("code"), QStringLiteral("Developer"),
                               QStringLiteral("repos"), QStringLiteral("work"),
                               QStringLiteral("workspace"), QStringLiteral("git")})
        roots << home + QLatin1Char('/') + rel;
    return roots;
}

bool isUnder(const QString &path, const QString &repo, const QStringList &roots)
{
    const auto within = [&](const QString &root) {
        const QString r = QDir::cleanPath(root);
        return path == r || path.startsWith(r + QLatin1Char('/'));
    };
    return within(repo) || std::any_of(roots.cbegin(), roots.cend(), within);
}

void collectRepos(const QStringList &roots, QSet<QString> *repos)
{
    for (const QString &root : roots) {
        if (isRepo(root))
            repos->insert(QFileInfo(root).absoluteFilePath());
        const QDir dir(root);
        for (const QString &child : dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks)) {
            const QString p = dir.filePath(child);
            if (isRepo(p))
                repos->insert(QFileInfo(p).absoluteFilePath());
        }
    }
}

} // namespace

ScanOptions ScanOptions::forHome(const QString &home)
{
    ScanOptions o;
    o.repoSearchRoots = defaultRepoSearchRoots(home);
    for (const QString &name : agentFolderNames())
        o.agentRoots << home + QLatin1Char('/') + name + QStringLiteral("/worktrees");
    return o;
}

bool isAgentWorktreePath(const QString &path)
{
    const QString p = QDir::cleanPath(path);
    for (const QString &name : agentFolderNames()) {
        if (p.contains(QLatin1Char('/') + name + QStringLiteral("/worktrees/")))
            return true;
    }
    return false;
}

bool gitAvailable()
{
    if (!CommandUtil::isExecutable(QStringLiteral("git")))
        return false;
#ifdef Q_OS_MACOS
    // /usr/bin/git is a shim that opens the Command Line Tools installer.
    if (!CommandUtil::execWithStatus(QStringLiteral("xcode-select"), {QStringLiteral("-p")}).ok()
        && !QFileInfo::exists(QStringLiteral("/opt/homebrew/bin/git"))
        && !QFileInfo::exists(QStringLiteral("/usr/local/bin/git")))
        return false;
#endif
    return true;
}

QString blockReason(const Worktree &wt)
{
    if (wt.inUse)
        return tr("A process is running inside this folder.");
    if (wt.kind == Worktree::Kind::Idle) {
        if (wt.dirtyCount < 0)
            return tr("Its status could not be read.");
        if (wt.dirtyCount > 0)
            return tr("It has uncommitted changes.");
    }
    return QString();
}

QList<Worktree> scan(const ScanOptions &options)
{
    QList<Worktree> rows;
    QSet<QString> seen;
    const bool haveGit = gitAvailable();

    QSet<QString> repos;
    collectRepos(options.repoSearchRoots, &repos);

    QStringList roots = options.agentRoots;
    for (const QString &repo : std::as_const(repos)) {
        for (const QString &name : agentFolderNames())
            roots << repo + QLatin1Char('/') + name + QStringLiteral("/worktrees");
    }

    for (const QString &root : std::as_const(roots)) {
        for (const QString &folder : findLinkedFolders(root, kAgentRootDepth)) {
            const LinkedFolder lf = inspectFolder(folder);
            if (!lf.linked)
                continue;
            if (lf.registered) {
                if (!lf.repoPath.isEmpty())
                    repos.insert(lf.repoPath);
                continue;
            }
            if (seen.contains(folder))
                continue;
            seen.insert(folder);
            Worktree wt;
            wt.kind = Worktree::Kind::Unregistered;
            wt.path = folder;
            wt.repoPath = lf.repoPath;
            wt.folderExists = true;
            wt.sizeBytes = folderSize(folder);
            wt.lastActivity = QFileInfo(folder).lastModified();
            wt.inUse = probe(options.inUse, folder);
            rows.append(wt);
        }
    }

    if (haveGit) {
        const QDateTime now = QDateTime::currentDateTime();
        for (const QString &repo : std::as_const(repos)) {
            QList<ListedWorktree> listed;
            if (!listWorktrees(repo, &listed))
                continue;
            for (const ListedWorktree &lw : std::as_const(listed)) {
                if (lw.locked || !isAgentWorktreePath(lw.path) || seen.contains(lw.path))
                    continue;
                seen.insert(lw.path);

                Worktree wt;
                wt.path = lw.path;
                wt.repoPath = repo;
                wt.branch = lw.branch;
                wt.folderExists = QFileInfo(lw.path).isDir();

                if (!wt.folderExists) {
                    // Inside the repo or a known agent folder, a vanished parent just
                    // means the agent folder was deleted. Elsewhere it may be an
                    // unmounted drive, which git also reports as prunable.
                    if (!isUnder(lw.path, repo, options.agentRoots)
                        && !QFileInfo(QFileInfo(lw.path).absolutePath()).isDir())
                        continue;
                    wt.kind = Worktree::Kind::MissingFolder;
                    rows.append(wt);
                    continue;
                }

                const LinkedFolder lf = inspectFolder(lw.path);
                if (!lf.linked || !lf.registered)
                    continue;
                wt.lastActivity = lastActivityOf(lw.path, lf.admin);
                if (wt.lastActivity.daysTo(now) < options.idleDays)
                    continue;
                wt.kind = Worktree::Kind::Idle;
                wt.dirtyCount = dirtyCountOf(lw.path);
                wt.unpushedCount = unpushedCountOf(lw.path);
                wt.sizeBytes = folderSize(lw.path);
                wt.inUse = probe(options.inUse, lw.path);
                rows.append(wt);
            }
        }
    }

    std::sort(rows.begin(), rows.end(), [](const Worktree &a, const Worktree &b) {
        return a.path < b.path;
    });
    return rows;
}

namespace {

RemoveResult refuse(const QString &message)
{
    return {false, message};
}

void audit(const Worktree &wt, LifecycleAuditLog::Action action, const QString &ruleId,
           const QString &canonical, const QString &trashDestination)
{
    LifecycleAuditLog::Entry entry;
    entry.timestamp = QDateTime::currentDateTimeUtc();
    entry.batchId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    entry.originalPath = wt.path;
    entry.canonicalPath = canonical;
    entry.action = action;
    entry.trashDestination = trashDestination;
    entry.matchingRuleIds = {ruleId};
    entry.sizeBytes = wt.sizeBytes;
    entry.nexisVersion = QCoreApplication::applicationVersion();
    LifecycleAuditLog::append(entry);
}

bool stillListed(const Worktree &wt)
{
    QList<ListedWorktree> listed;
    if (wt.repoPath.isEmpty() || !listWorktrees(wt.repoPath, &listed))
        return false;
    const QString want = QDir::cleanPath(wt.path);
    return std::any_of(listed.cbegin(), listed.cend(), [&](const ListedWorktree &lw) {
        return lw.path == want && !lw.locked;
    });
}

RemoveResult gitRemove(const Worktree &wt)
{
    const ExecResult r = runGit(wt.repoPath, {QStringLiteral("worktree"), QStringLiteral("remove"),
                                              QStringLiteral("--"), wt.path});
    if (!r.ok())
        return refuse(tr("git did not remove it: %1").arg(firstLine(r.error)));
    return {true, QString()};
}

} // namespace

RemoveResult removeWorktree(const Worktree &wt, const RemoveOptions &options)
{
    const QStringList args = options.appArgumentsSet ? options.appArguments
                                                     : QCoreApplication::arguments();
    if (HeadlessUtil::isHeadlessArguments(args))
        return refuse(tr("Worktrees are only removed from an interactive session."));

    if (wt.kind != Worktree::Kind::Unregistered && !gitAvailable())
        return refuse(tr("git is not available."));

    switch (wt.kind) {
    case Worktree::Kind::MissingFolder: {
        if (QFileInfo::exists(wt.path))
            return refuse(tr("The folder has reappeared, so nothing was removed."));
        if (!stillListed(wt))
            return refuse(tr("The repository no longer lists this worktree."));
        return gitRemove(wt);
    }
    case Worktree::Kind::Idle: {
        if (!QFileInfo(wt.path).isDir() || !stillListed(wt))
            return refuse(tr("The repository no longer lists this worktree."));
        const QString canonical = QFileInfo(wt.path).canonicalFilePath();
        if (canonical.isEmpty() || !LifecycleDenyList::isSafe(canonical))
            return refuse(tr("This location is protected and cannot be removed."));
        if (probe(options.inUse, wt.path))
            return refuse(tr("A process is running inside this folder."));
        const int dirty = dirtyCountOf(wt.path);
        if (dirty != 0)
            return refuse(dirty < 0 ? tr("Its status could not be read.")
                                    : tr("It has uncommitted changes."));
        const RemoveResult r = gitRemove(wt);
        if (r.ok)
            audit(wt, LifecycleAuditLog::Action::PermanentlyDeleted,
                  QStringLiteral("git-worktree-idle"), canonical, QString());
        return r;
    }
    case Worktree::Kind::Unregistered: {
        const LinkedFolder lf = inspectFolder(wt.path);
        if (!lf.linked || lf.registered)
            return refuse(tr("This folder is no longer an orphaned worktree."));
        const QString canonical = QFileInfo(wt.path).canonicalFilePath();
        if (canonical.isEmpty() || !LifecycleDenyList::isSafe(canonical))
            return refuse(tr("This location is protected and cannot be removed."));
        if (probe(options.inUse, wt.path))
            return refuse(tr("A process is running inside this folder."));
        QString trashDest;
        const bool moved = options.moveToTrash ? options.moveToTrash(wt.path, &trashDest)
                                                : QFile::moveToTrash(wt.path, &trashDest);
        if (!moved)
            return refuse(tr("The folder could not be moved to the Trash."));
        audit(wt, LifecycleAuditLog::Action::MovedToTrash,
              QStringLiteral("git-worktree-unregistered"), canonical, trashDest);
        return {true, QString()};
    }
    }
    return refuse(QString());
}

bool isPathInUseProc(const QString &procRoot, const QString &path, qint64 ownPid)
{
    const QDir proc(procRoot);
    if (!proc.exists())
        return true;

    QString target = QFileInfo(path).canonicalFilePath();
    if (target.isEmpty())
        target = QDir::cleanPath(path);

    const QStringList pids = proc.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString &name : pids) {
        bool numeric = false;
        const qint64 pid = name.toLongLong(&numeric);
        if (!numeric || pid == ownPid)
            continue;
        const QString cwd = QFileInfo(proc.filePath(name + QStringLiteral("/cwd"))).symLinkTarget();
        if (cwd == target || cwd.startsWith(target + QLatin1Char('/')))
            return true;
    }
    return false;
}

QStringList parseLsofCwdPaths(const QString &lsofFieldOutput)
{
    QStringList paths;
    for (const QString &line : lsofFieldOutput.split(QLatin1Char('\n'))) {
        if (line.startsWith(QLatin1Char('n')) && line.size() > 1)
            paths << line.mid(1);
    }
    return paths;
}

bool isPathInUse(const QString &path)
{
#ifdef Q_OS_MACOS
    if (!CommandUtil::isExecutable(QStringLiteral("lsof")))
        return true;
    const ExecResult r = CommandUtil::execWithStatus(
        QStringLiteral("lsof"), {QStringLiteral("-Fn"), QStringLiteral("-d"), QStringLiteral("cwd")});
    if (r.output.isEmpty() && !r.error.isEmpty())
        return true;
    QString target = QFileInfo(path).canonicalFilePath();
    if (target.isEmpty())
        target = QDir::cleanPath(path);
    const QStringList cwds = parseLsofCwdPaths(r.output);
    return std::any_of(cwds.cbegin(), cwds.cend(), [&](const QString &cwd) {
        return cwd == target || cwd.startsWith(target + QLatin1Char('/'));
    });
#else
    return isPathInUseProc(QStringLiteral("/proc"), path, QCoreApplication::applicationPid());
#endif
}

} // namespace GitWorktreeTool
