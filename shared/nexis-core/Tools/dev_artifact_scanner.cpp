#include "dev_artifact_scanner.h"

#include "lifecycle_deny_list.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QSet>
#include <QTextStream>

#include <algorithm>

#if defined(Q_OS_UNIX)
#include <climits>
#include <unistd.h>
#endif

namespace DevArtifacts {

namespace {

struct ArtifactRule {
    const char *dir;
    const char *mustContain;   // a file that proves the folder is what we think it is
};

struct ProjectType {
    const char *name;
    QList<const char *> markers;
    QList<ArtifactRule> artifacts;
};

const QList<ProjectType> &projectTypes()
{
    static const QList<ProjectType> kTypes = {
        {"Node.js",     {"package.json"}, {{"node_modules", nullptr}}},
        {"Rust",        {"Cargo.toml"}, {{"target", nullptr}}},
        {"Maven",       {"pom.xml"}, {{"target", nullptr}}},
        {"Python",      {"pyproject.toml", "requirements.txt", "setup.py", "Pipfile"},
                        {{".venv", "pyvenv.cfg"}, {"venv", "pyvenv.cfg"}}},
        {"Gradle",      {"build.gradle", "build.gradle.kts", "settings.gradle",
                         "settings.gradle.kts"},
                        {{".gradle", nullptr}, {"build", nullptr}}},
        {"CocoaPods",   {"Podfile"}, {{"Pods", nullptr}}},
        {"Dart/Flutter", {"pubspec.yaml"}, {{"build", nullptr}, {".dart_tool", nullptr}}},
    };
    return kTypes;
}

// Directory names the discovery walk never descends into: every artifact name
// above (rebuildable output, and a poor place to look for projects) plus
// node_modules.
bool isArtifactDirName(const QString &name)
{
    static const QSet<QString> kNames = [] {
        QSet<QString> s;
        for (const ProjectType &t : projectTypes())
            for (const ArtifactRule &a : t.artifacts)
                s.insert(QString::fromLatin1(a.dir));
        return s;
    }();
    return kNames.contains(name);
}

QString join(const QString &dir, const QString &leaf)
{
    return dir + QLatin1Char('/') + leaf;
}

bool isUnder(const QString &path, const QString &root)
{
    return path == root || path.startsWith(root + QLatin1Char('/'));
}

QString canonical(const QString &path)
{
    return QFileInfo(path).canonicalFilePath();
}

QString homeCanonical(const QString &homeOverride)
{
    const QString raw = homeOverride.isEmpty() ? QDir::homePath() : homeOverride;
    const QString c = canonical(raw);
    return c.isEmpty() ? QDir::cleanPath(raw) : c;
}

bool ownedByCurrentUser(const QFileInfo &info)
{
#if defined(Q_OS_UNIX)
    return info.ownerId() == geteuid();
#else
    Q_UNUSED(info);
    return true;
#endif
}

struct GitLocation {
    QString gitDir;     // directory holding index/HEAD (per-worktree for linked worktrees)
    QString workTree;   // directory that contains the .git entry
};

GitLocation locateGit(const QString &start)
{
    QDir dir(start);
    for (int hops = 0; hops < 40; ++hops) {
        const QString dotGit = dir.filePath(QStringLiteral(".git"));
        const QFileInfo info(dotGit);
        if (info.isDir())
            return {dotGit, dir.absolutePath()};
        if (info.isFile()) {
            QFile f(dotGit);
            if (f.open(QIODevice::ReadOnly)) {
                const QString line = QString::fromUtf8(f.readLine()).trimmed();
                const QString prefix = QStringLiteral("gitdir:");
                if (line.startsWith(prefix)) {
                    const QString target = line.mid(prefix.size()).trimmed();
                    const QString resolved = QDir::isAbsolutePath(target)
                        ? target : dir.absoluteFilePath(target);
                    return {QDir::cleanPath(resolved), dir.absolutePath()};
                }
            }
            return {};
        }
        if (!dir.cdUp())
            break;
    }
    return {};
}

// The directory whose processes count as "using" the project. A package inside
// a monorepo shares one git work tree, so a dev server started at the repo root
// holds the sub-package's node_modules even though its cwd is not inside the
// package. A work tree that is $HOME (or contains it) says nothing about the
// project, so it is ignored.
QString useGuardRoot(const QString &projectRoot, const QString &home)
{
    const GitLocation git = locateGit(projectRoot);
    if (git.workTree.isEmpty())
        return projectRoot;
    const QString workTree = canonical(git.workTree);
    if (workTree.isEmpty() || workTree == home || home.startsWith(workTree + QLatin1Char('/')))
        return projectRoot;
    return isUnder(projectRoot, workTree) ? workTree : projectRoot;
}

struct Candidate {
    QString path;
    QString kind;
};

// The rebuildable folders that sit directly in `dir` for every project type
// whose marker is present. Real directories only: a symlink named node_modules
// is never offered.
QList<Candidate> candidateArtifacts(const QString &dir)
{
    QList<Candidate> found;
    QSet<QString> seen;
    for (const ProjectType &type : projectTypes()) {
        bool hasMarker = false;
        for (const char *marker : type.markers) {
            if (QFileInfo(join(dir, QString::fromLatin1(marker))).isFile()) {
                hasMarker = true;
                break;
            }
        }
        if (!hasMarker)
            continue;
        for (const ArtifactRule &rule : type.artifacts) {
            const QString path = join(dir, QString::fromLatin1(rule.dir));
            const QFileInfo info(path);
            if (info.isSymLink() || !info.isDir())
                continue;
            if (rule.mustContain && !QFileInfo::exists(join(path, QString::fromLatin1(rule.mustContain))))
                continue;
            if (seen.contains(path))
                continue;
            seen.insert(path);
            found.append({path, QString::fromLatin1(type.name)});
        }
    }
    return found;
}

quint64 directoryBytes(const QString &path)
{
    quint64 total = 0;
    QDirIterator it(path, QDir::Files | QDir::Hidden | QDir::NoSymLinks,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        total += static_cast<quint64>(it.fileInfo().size());
    }
    return total;
}

#if defined(Q_OS_LINUX)

QString readLink(const QString &path)
{
    char buf[PATH_MAX];
    const ssize_t n = ::readlink(QFile::encodeName(path).constData(), buf, sizeof(buf) - 1);
    if (n <= 0)
        return {};
    return QFile::decodeName(QByteArray(buf, static_cast<int>(n)));
}

void addHeld(QStringList &held, QString target)
{
    static const QString kDeleted = QStringLiteral(" (deleted)");
    if (target.endsWith(kDeleted))
        target.chop(kDeleted.size());
    if (target.startsWith(QLatin1Char('/')))
        held.append(target);
}

#endif

} // namespace

quint64 Project::totalBytes() const
{
    quint64 total = 0;
    for (const Artifact &a : artifacts)
        total += a.bytes;
    return total;
}

qint64 Project::idleDays(const QDateTime &now) const
{
    return lastActivity.isValid() ? lastActivity.daysTo(now) : 0;
}

HeldPathsProbe::HeldPathsProbe(const QStringList &heldPaths, bool available)
    : mHeld(heldPaths), mAvailable(available)
{
    std::sort(mHeld.begin(), mHeld.end());
}

HeldPathsProbe HeldPathsProbe::fromSystem()
{
#if defined(Q_OS_LINUX)
    if (readLink(QStringLiteral("/proc/self/cwd")).isEmpty())
        return HeldPathsProbe({}, false);

    QStringList held;
    const QDir proc(QStringLiteral("/proc"));
    const QStringList pids = proc.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString &pid : pids) {
        bool numeric = false;
        pid.toUInt(&numeric);
        if (!numeric)
            continue;
        const QString base = QStringLiteral("/proc/") + pid;
        addHeld(held, readLink(base + QStringLiteral("/cwd")));
        addHeld(held, readLink(base + QStringLiteral("/exe")));
        const QDir fdDir(base + QStringLiteral("/fd"));
        const QStringList fds = fdDir.entryList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::System);
        for (const QString &fd : fds)
            addHeld(held, readLink(fdDir.filePath(fd)));
    }
    return HeldPathsProbe(held, true);
#elif defined(Q_OS_MAC)
    QProcess lsof;
    lsof.start(QStringLiteral("lsof"), {QStringLiteral("-nP"), QStringLiteral("-w"), QStringLiteral("-Fn")});
    if (!lsof.waitForStarted(5000))
        return HeldPathsProbe({}, false);
    if (!lsof.waitForFinished(30000)) {
        lsof.kill();
        lsof.waitForFinished(2000);
        return HeldPathsProbe({}, false);
    }
    QStringList held;
    QTextStream out(lsof.readAllStandardOutput());
    while (!out.atEnd()) {
        const QString line = out.readLine();
        if (line.size() > 1 && line.at(0) == QLatin1Char('n') && line.at(1) == QLatin1Char('/'))
            held.append(line.mid(1));
    }
    // lsof exits non-zero when some processes are not inspectable but still
    // lists the rest; an empty listing means it did not work at all.
    return HeldPathsProbe(held, !held.isEmpty());
#else
    return HeldPathsProbe({}, false);
#endif
}

bool HeldPathsProbe::holdsPathUnder(const QString &canonicalDir) const
{
    const QString prefix = canonicalDir + QLatin1Char('/');
    const auto it = std::lower_bound(mHeld.begin(), mHeld.end(), prefix);
    if (it != mHeld.end() && it->startsWith(prefix))
        return true;
    return std::binary_search(mHeld.begin(), mHeld.end(), canonicalDir);
}

QString rootRefusalReason(const QString &path, const QString &homeOverride)
{
    if (path.trimmed().isEmpty())
        return QStringLiteral("empty path");
    const QString resolved = canonical(path);
    if (resolved.isEmpty())
        return QStringLiteral("folder does not exist");
    if (!QFileInfo(resolved).isDir())
        return QStringLiteral("not a folder");
    if (resolved == QLatin1String("/"))
        return QStringLiteral("filesystem root");
    const QString home = homeCanonical(homeOverride);
    if (resolved == home)
        return QStringLiteral("your home folder");
    if (home.startsWith(resolved + QLatin1Char('/')))
        return QStringLiteral("a folder that contains your home folder");
    if (!LifecycleDenyList::isSafeForCleaning(resolved))
        return QStringLiteral("protected system location");
    return {};
}

bool gitOperationInProgress(const QString &projectRoot)
{
    const GitLocation git = locateGit(projectRoot);
    if (git.gitDir.isEmpty())
        return false;

    static const QStringList kInProgress = {
        QStringLiteral("index.lock"),      QStringLiteral("HEAD.lock"),
        QStringLiteral("MERGE_HEAD"),      QStringLiteral("CHERRY_PICK_HEAD"),
        QStringLiteral("REVERT_HEAD"),     QStringLiteral("REBASE_HEAD"),
        QStringLiteral("BISECT_LOG"),      QStringLiteral("BISECT_START"),
        QStringLiteral("rebase-merge"),    QStringLiteral("rebase-apply"),
        QStringLiteral("sequencer"),
    };
    for (const QString &name : kInProgress) {
        if (QFileInfo::exists(join(git.gitDir, name)))
            return true;
    }

    // Linked worktrees keep shared state (packed refs, shallow, gc) in the
    // common directory.
    QString commonDir = git.gitDir;
    QFile commonFile(join(git.gitDir, QStringLiteral("commondir")));
    if (commonFile.open(QIODevice::ReadOnly)) {
        const QString rel = QString::fromUtf8(commonFile.readLine()).trimmed();
        if (!rel.isEmpty())
            commonDir = QDir::cleanPath(QDir::isAbsolutePath(rel) ? rel : join(git.gitDir, rel));
    }
    static const QStringList kShared = {
        QStringLiteral("shallow.lock"), QStringLiteral("packed-refs.lock"),
        QStringLiteral("gc.pid"),
    };
    for (const QString &name : kShared) {
        if (QFileInfo::exists(join(commonDir, name)))
            return true;
    }
    return false;
}

QDateTime lastActivity(const QString &projectRoot, const QStringList &ignoredDirs, int maxEntries)
{
    QDateTime newest;
    auto consider = [&newest](const QDateTime &t) {
        if (t.isValid() && (!newest.isValid() || t > newest))
            newest = t;
    };

    const QSet<QString> ignored(ignoredDirs.begin(), ignoredDirs.end());
    int seen = 0;
    QStringList stack{projectRoot};
    while (!stack.isEmpty()) {
        const QDir dir(stack.takeLast());
        const QFileInfoList entries = dir.entryInfoList(
            QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::NoSymLinks, QDir::Unsorted);
        for (const QFileInfo &entry : entries) {
            if (++seen > maxEntries)
                return {};
            if (entry.isDir()) {
                const QString name = entry.fileName();
                if (name == QLatin1String(".git") || name == QLatin1String("node_modules")
                    || ignored.contains(entry.absoluteFilePath()))
                    continue;
                stack.append(entry.absoluteFilePath());
            } else if (entry.isFile()) {
                consider(entry.lastModified());
            }
        }
    }

    const GitLocation git = locateGit(projectRoot);
    if (!git.gitDir.isEmpty()) {
        for (const QString &name : {QStringLiteral("index"), QStringLiteral("HEAD"),
                                    QStringLiteral("ORIG_HEAD"), QStringLiteral("logs/HEAD")}) {
            const QFileInfo info(join(git.gitDir, name));
            if (info.exists())
                consider(info.lastModified());
        }
    }
    return newest;
}

ScanResult scan(const ScanOptions &options, const UseProbe &probe, const QAtomicInt *cancel)
{
    ScanResult result;
    if (!probe.available()) {
        result.probeUnavailable = true;
        return result;
    }

    const QString home = homeCanonical(options.homeOverride);
    auto isCancelled = [cancel] { return cancel && cancel->loadRelaxed() != 0; };
    auto isExcluded = [&options](const QString &path) {
        return options.isExcluded && options.isExcluded(path);
    };

    QStringList roots;
    for (const QString &raw : options.roots) {
        const QString reason = rootRefusalReason(raw, home);
        if (!reason.isEmpty()) {
            result.refusedRoots.append(raw + QStringLiteral(": ") + reason);
            continue;
        }
        const QString resolved = canonical(raw);
        if (!roots.contains(resolved))
            roots.append(resolved);
    }

    struct Pending {
        QString dir;
        int depth;
    };
    QSet<QString> seenProjects;
    int directories = 0;

    for (const QString &root : roots) {
        QList<Pending> stack{{root, 0}};
        while (!stack.isEmpty()) {
            if (isCancelled()) {
                result.cancelled = true;
                return result;
            }
            if (++directories > options.maxDirectories)
                return result;

            const Pending current = stack.takeLast();
            const QString &dir = current.dir;

            const QList<Candidate> candidates = candidateArtifacts(dir);
            if (!candidates.isEmpty() && !seenProjects.contains(dir)) {
                seenProjects.insert(dir);
                [&] {
                    const QString refusal = rootRefusalReason(dir, home);
                    if (!refusal.isEmpty()) {
                        result.refusedRoots.append(dir + QStringLiteral(": ") + refusal);
                        return;
                    }
                    if (isExcluded(dir)) {
                        ++result.skippedExcluded;
                        return;
                    }

                    Project project;
                    project.root = dir;
                    project.name = QFileInfo(dir).fileName();
                    QStringList artifactPaths;
                    for (const Candidate &c : candidates)
                        artifactPaths.append(c.path);
                    for (const Candidate &c : candidates) {
                        const QFileInfo info(c.path);
                        const QString resolved = info.canonicalFilePath();
                        if (resolved.isEmpty() || !resolved.startsWith(dir + QLatin1Char('/')))
                            continue;
                        if (!LifecycleDenyList::isSafeForCleaning(resolved) || !ownedByCurrentUser(info))
                            continue;
                        if (isExcluded(resolved))
                            continue;
                        project.artifacts.append({resolved, 0});
                        if (!project.kinds.contains(c.kind))
                            project.kinds.append(c.kind);
                    }
                    if (project.artifacts.isEmpty()) {
                        ++result.skippedExcluded;
                        return;
                    }

                    if (gitOperationInProgress(dir)) {
                        ++result.skippedGitOperation;
                        return;
                    }
                    const QString guard = useGuardRoot(dir, home);
                    if (probe.holdsPathUnder(dir) || (guard != dir && probe.holdsPathUnder(guard))) {
                        ++result.skippedInUse;
                        return;
                    }

                    project.lastActivity = lastActivity(dir, artifactPaths, options.maxActivityEntries);
                    if (!project.lastActivity.isValid()) {
                        ++result.skippedUnverifiable;
                        return;
                    }
                    for (Artifact &a : project.artifacts)
                        a.bytes = directoryBytes(a.path);
                    result.projects.append(project);
                }();
            }

            if (current.depth >= options.maxDepth)
                continue;
            const QFileInfoList subdirs = QDir(dir).entryInfoList(
                QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks, QDir::Unsorted);
            for (const QFileInfo &sub : subdirs) {
                if (isArtifactDirName(sub.fileName()))
                    continue;
                stack.append({sub.absoluteFilePath(), current.depth + 1});
            }
        }
    }

    std::sort(result.projects.begin(), result.projects.end(),
              [](const Project &a, const Project &b) { return a.totalBytes() > b.totalBytes(); });
    return result;
}

QList<TrashOutcome> trashArtifacts(const QList<TrashRequest> &requests,
                                   RunContext context,
                                   const UseProbe &probe,
                                   const std::function<bool(const QString &)> &moveToTrash,
                                   const std::function<bool(const QString &)> &isExcluded,
                                   const QString &homeOverride)
{
    QList<TrashOutcome> outcomes;
    const QString home = homeCanonical(homeOverride);
    auto excluded = [&isExcluded](const QString &path) { return isExcluded && isExcluded(path); };

    for (const TrashRequest &request : requests) {
        TrashOutcome outcome;
        outcome.request = request;
        outcome.reason = [&]() -> QString {
            if (context != RunContext::Interactive)
                return QStringLiteral("Developer-project cleanup runs only from the app window, "
                                      "never in scheduled or headless runs.");
            if (!probe.available())
                return QStringLiteral("Could not check whether a process is using the project.");

            const QString root = canonical(request.projectRoot);
            if (root.isEmpty())
                return QStringLiteral("The project folder no longer exists.");
            const QString rootRefusal = rootRefusalReason(root, home);
            if (!rootRefusal.isEmpty())
                return QStringLiteral("Refused: project folder is ") + rootRefusal + QLatin1Char('.');

            const QFileInfo artifact(request.artifactPath);
            if (artifact.isSymLink() || !artifact.isDir())
                return QStringLiteral("No longer a regular folder.");
            const QString resolved = artifact.canonicalFilePath();
            if (resolved.isEmpty() || QFileInfo(resolved).absolutePath() != root)
                return QStringLiteral("Not directly inside the project folder.");

            // The folder must still be a rebuildable folder of a project type
            // whose marker is still present — not just any directory a stale
            // listing happens to name.
            const QList<Candidate> candidates = candidateArtifacts(root);
            QStringList artifactPaths;
            bool allowed = false;
            for (const Candidate &c : candidates) {
                artifactPaths.append(c.path);
                if (c.path == resolved)
                    allowed = true;
            }
            if (!allowed)
                return QStringLiteral("No longer matches a project's rebuildable folder.");

            if (!LifecycleDenyList::isSafeForCleaning(resolved))
                return QStringLiteral("Refused: protected location.");
            if (!ownedByCurrentUser(QFileInfo(resolved)))
                return QStringLiteral("Refused: not owned by you.");
            if (excluded(resolved) || excluded(root))
                return QStringLiteral("Excluded by your exclusion list.");
            if (gitOperationInProgress(root))
                return QStringLiteral("A git operation is in progress.");
            const QString guard = useGuardRoot(root, home);
            if (probe.holdsPathUnder(root) || (guard != root && probe.holdsPathUnder(guard)))
                return QStringLiteral("A running process is using the project.");

            const QDateTime activity = lastActivity(root, artifactPaths);
            if (!activity.isValid())
                return QStringLiteral("Project is too large to verify.");
            if (request.scannedActivity.isValid() && activity > request.scannedActivity)
                return QStringLiteral("The project changed since the scan.");

            return moveToTrash(resolved) ? QString() : QStringLiteral("Could not move to the Trash.");
        }();
        outcome.trashed = outcome.reason.isEmpty();
        outcomes.append(outcome);
    }
    return outcomes;
}

} // namespace DevArtifacts
