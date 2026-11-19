// SSO-27098: stale developer-project artifact cleaner. Covers the pure core in
// Tools/dev_artifact_scanner: project discovery by marker file, age from real
// project activity, the in-use / git-operation skips, $HOME and deny-list
// refusal, and the re-validated, interactive-only move to Trash.

#include <QTest>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QTemporaryDir>

#include "Tools/dev_artifact_scanner.h"

using namespace DevArtifacts;

namespace {

void writeFile(const QString &path, const QByteArray &content = "x")
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    QVERIFY2(f.open(QIODevice::WriteOnly), qPrintable(path));
    f.write(content);
}

void setMtime(const QString &path, const QDateTime &when)
{
    QFile f(path);
    QVERIFY2(f.open(QIODevice::ReadWrite), qPrintable(path));
    QVERIFY(f.setFileTime(when, QFile::FileModificationTime));
}

bool sameSecond(const QDateTime &a, const QDateTime &b)
{
    return a.isValid() && b.isValid() && qAbs(a.secsTo(b)) <= 1;
}

HeldPathsProbe nothingHeld() { return HeldPathsProbe({}, true); }

} // namespace

class TestDevArtifactScanner : public QObject
{
    Q_OBJECT

private slots:
    void init();

    // Age
    void lastActivity_ignoresArtifactFolderMtime();
    void lastActivity_includesGitIndex();
    void lastActivity_ignoresGitObjects();
    void lastActivity_exceedingEntryCapIsUnverifiable();
    void idleDays_countsFromLastActivity();

    // Discovery
    void scan_oneRowPerProjectWithAllArtifacts();
    void scan_pythonVenvNeedsPyvenvCfg();
    void scan_noMarkerNoRow();
    void scan_symlinkedArtifactIsNeverOffered();
    void scan_doesNotDescendIntoArtifactFolders();
    void scan_exclusionCallbackHidesProject();
    void scan_sortsLargestFirst();

    // In use
    void scan_projectWithProcessInsideIsNotListed();
    void scan_sameNamePrefixSiblingIsNotInUse();
    void scan_probeUnavailableListsNothing();
    void scan_monorepoPackageGuardedByRepoRoot();
    void heldPathsProbe_matchesDirectoryAndDescendantsOnly();
#if defined(Q_OS_LINUX) || defined(Q_OS_MAC)   // /proc on Linux, lsof on macOS
    void fromSystem_seesProcessWithCwdInsideProject();
#endif

    // Git operation
    void scan_gitOperationInProgressIsNotListed_data();
    void scan_gitOperationInProgressIsNotListed();
    void scan_linkedWorktreeGitFileIsHonoured();

    // $HOME / deny-list / symlinks
    void rootRefusalReason_refusesHomeAndAncestorsAndRoot();
    void scan_homeAsRootIsRefused();
    void scan_markerInHomeNeverMakesHomeAProject();
    void scan_symlinkRootIntoDeniedLocationIsRefused();
    void scan_symlinkRootIsResolvedBeforeJudging();

    // Trash
    void trash_interactiveMovesResolvedFolder();
    void trash_scheduledAndHeadlessAreRefused_data();
    void trash_scheduledAndHeadlessAreRefused();
    void trash_refusesWhenProcessAppearedSinceScan();
    void trash_refusesWhenProjectChangedSinceScan();
    void trash_refusesNonArtifactFolder();
    void trash_refusesSymlinkedArtifact();
    void trash_refusesGitOperationAndExclusionAndHome();
    void trash_refusesWhenProbeUnavailable();
    void trash_failedMoveIsReported();

private:
    // A Node project: package.json + src file + node_modules, all dated `activity`
    // (node_modules deliberately fresh).
    QString makeNodeProject(const QString &name, const QDateTime &activity,
                            int nodeModulesBytes = 1000);
    ScanOptions options() const;

    QTemporaryDir mTmp;
    QString mBase;   // canonical temp dir
    QString mWork;   // scan root
    QString mHome;   // fake $HOME
};

void TestDevArtifactScanner::init()
{
    QVERIFY(mTmp.isValid());
    mBase = QFileInfo(mTmp.path()).canonicalFilePath();
    mWork = mBase + "/work";
    mHome = mBase + "/home";
    QDir(mBase).removeRecursively();
    QVERIFY(QDir().mkpath(mWork));
    QVERIFY(QDir().mkpath(mHome));
}

QString TestDevArtifactScanner::makeNodeProject(const QString &name, const QDateTime &activity,
                                                int nodeModulesBytes)
{
    const QString root = mWork + "/" + name;
    writeFile(root + "/package.json", "{}");
    writeFile(root + "/src/index.js", "x");
    setMtime(root + "/package.json", activity);
    setMtime(root + "/src/index.js", activity);
    writeFile(root + "/node_modules/dep/index.js", QByteArray(nodeModulesBytes, 'x'));
    return root;
}

ScanOptions TestDevArtifactScanner::options() const
{
    ScanOptions o;
    o.roots = {mWork};
    o.homeOverride = mHome;
    return o;
}

// ── Age ──────────────────────────────────────────────────────────────────────

void TestDevArtifactScanner::lastActivity_ignoresArtifactFolderMtime()
{
    const QDateTime old = QDateTime::currentDateTime().addDays(-90);
    const QString root = makeNodeProject("app", old);
    writeFile(root + "/.venv/pyvenv.cfg");          // fresh, created just now
    writeFile(root + "/build/out.o");

    const QDateTime activity = lastActivity(root, {root + "/.venv", root + "/build"});
    QVERIFY2(sameSecond(activity, old), qPrintable(activity.toString()));
}

void TestDevArtifactScanner::lastActivity_includesGitIndex()
{
    const QDateTime old = QDateTime::currentDateTime().addDays(-90);
    const QDateTime recent = QDateTime::currentDateTime().addDays(-5);
    const QString root = makeNodeProject("app", old);
    writeFile(root + "/.git/HEAD", "ref: refs/heads/main\n");
    writeFile(root + "/.git/index");
    setMtime(root + "/.git/HEAD", old);
    setMtime(root + "/.git/index", recent);

    QVERIFY(sameSecond(lastActivity(root), recent));
}

void TestDevArtifactScanner::lastActivity_ignoresGitObjects()
{
    const QDateTime old = QDateTime::currentDateTime().addDays(-90);
    const QString root = makeNodeProject("app", old);
    writeFile(root + "/.git/HEAD", "ref: refs/heads/main\n");
    writeFile(root + "/.git/index");
    setMtime(root + "/.git/HEAD", old);
    setMtime(root + "/.git/index", old);
    writeFile(root + "/.git/objects/ab/cdef");      // fresh pack churn, not activity

    QVERIFY(sameSecond(lastActivity(root), old));
}

void TestDevArtifactScanner::lastActivity_exceedingEntryCapIsUnverifiable()
{
    const QString root = makeNodeProject("app", QDateTime::currentDateTime().addDays(-90));
    for (int i = 0; i < 20; ++i)
        writeFile(root + QString("/src/f%1.js").arg(i));
    QVERIFY(!lastActivity(root, {}, 5).isValid());
}

void TestDevArtifactScanner::idleDays_countsFromLastActivity()
{
    Project p;
    p.lastActivity = QDateTime::currentDateTime().addDays(-45);
    QCOMPARE(p.idleDays(), 45);
}

// ── Discovery ────────────────────────────────────────────────────────────────

void TestDevArtifactScanner::scan_oneRowPerProjectWithAllArtifacts()
{
    const QDateTime old = QDateTime::currentDateTime().addDays(-60);
    const QString root = makeNodeProject("poly", old, 500);
    writeFile(root + "/pyproject.toml");
    writeFile(root + "/.venv/pyvenv.cfg");
    writeFile(root + "/.venv/lib/x.py", QByteArray(300, 'x'));
    setMtime(root + "/pyproject.toml", old);

    const ScanResult r = scan(options(), nothingHeld());
    QCOMPARE(r.projects.size(), 1);
    const Project &p = r.projects.first();
    QCOMPARE(p.root, root);
    QCOMPARE(p.name, QString("poly"));
    QCOMPARE(p.artifacts.size(), 2);
    QVERIFY(p.kinds.contains("Node.js"));
    QVERIFY(p.kinds.contains("Python"));
    QCOMPARE(p.totalBytes(), quint64(500 + 300 + 1 /* pyvenv.cfg */));
    QVERIFY(sameSecond(p.lastActivity, old));
}

void TestDevArtifactScanner::scan_pythonVenvNeedsPyvenvCfg()
{
    const QString root = mWork + "/py";
    writeFile(root + "/requirements.txt");
    writeFile(root + "/venv/notes.txt");            // a folder that merely is called venv
    QVERIFY(scan(options(), nothingHeld()).projects.isEmpty());

    writeFile(root + "/venv/pyvenv.cfg");
    QCOMPARE(scan(options(), nothingHeld()).projects.size(), 1);
}

void TestDevArtifactScanner::scan_noMarkerNoRow()
{
    writeFile(mWork + "/loose/node_modules/dep/index.js");
    writeFile(mWork + "/loose/target/debug/x");
    QVERIFY(scan(options(), nothingHeld()).projects.isEmpty());
}

void TestDevArtifactScanner::scan_symlinkedArtifactIsNeverOffered()
{
    const QString root = mWork + "/linked";
    writeFile(root + "/package.json");
    writeFile(mBase + "/elsewhere/dep/index.js");
    QVERIFY(QFile::link(mBase + "/elsewhere", root + "/node_modules"));

    QVERIFY(scan(options(), nothingHeld()).projects.isEmpty());
}

void TestDevArtifactScanner::scan_doesNotDescendIntoArtifactFolders()
{
    makeNodeProject("outer", QDateTime::currentDateTime().addDays(-60));
    // A package.json inside node_modules must not become its own project row.
    writeFile(mWork + "/outer/node_modules/dep/package.json");
    writeFile(mWork + "/outer/node_modules/dep/node_modules/x/y.js");

    const ScanResult r = scan(options(), nothingHeld());
    QCOMPARE(r.projects.size(), 1);
    QCOMPARE(r.projects.first().name, QString("outer"));
}

void TestDevArtifactScanner::scan_exclusionCallbackHidesProject()
{
    const QString kept = makeNodeProject("kept", QDateTime::currentDateTime().addDays(-60));
    const QString hidden = makeNodeProject("hidden", QDateTime::currentDateTime().addDays(-60));
    ScanOptions o = options();
    o.isExcluded = [&](const QString &p) { return p == hidden || p.startsWith(hidden + "/"); };

    const ScanResult r = scan(o, nothingHeld());
    QCOMPARE(r.projects.size(), 1);
    QCOMPARE(r.projects.first().root, kept);
    QCOMPARE(r.skippedExcluded, 1);
}

void TestDevArtifactScanner::scan_sortsLargestFirst()
{
    const QDateTime old = QDateTime::currentDateTime().addDays(-60);
    makeNodeProject("small", old, 100);
    makeNodeProject("big", old, 5000);
    makeNodeProject("mid", old, 1000);

    const ScanResult r = scan(options(), nothingHeld());
    QCOMPARE(r.projects.size(), 3);
    QCOMPARE(r.projects.at(0).name, QString("big"));
    QCOMPARE(r.projects.at(1).name, QString("mid"));
    QCOMPARE(r.projects.at(2).name, QString("small"));
}

// ── In use ───────────────────────────────────────────────────────────────────

void TestDevArtifactScanner::scan_projectWithProcessInsideIsNotListed()
{
    const QDateTime old = QDateTime::currentDateTime().addDays(-60);
    const QString busy = makeNodeProject("busy", old);
    const QString idle = makeNodeProject("idle", old);

    // A process cwd inside the project, and an open file inside it, both count.
    for (const QString &held : {busy + "/src", busy + "/node_modules/dep/index.js"}) {
        const ScanResult r = scan(options(), HeldPathsProbe({held}, true));
        QCOMPARE(r.projects.size(), 1);
        QCOMPARE(r.projects.first().root, idle);
        QCOMPARE(r.skippedInUse, 1);
    }
}

void TestDevArtifactScanner::scan_sameNamePrefixSiblingIsNotInUse()
{
    const QDateTime old = QDateTime::currentDateTime().addDays(-60);
    makeNodeProject("app", old);
    makeNodeProject("app-two", old);

    const ScanResult r = scan(options(), HeldPathsProbe({mWork + "/app-two/src"}, true));
    QCOMPARE(r.projects.size(), 1);
    QCOMPARE(r.projects.first().name, QString("app"));
}

void TestDevArtifactScanner::scan_probeUnavailableListsNothing()
{
    makeNodeProject("app", QDateTime::currentDateTime().addDays(-60));
    const ScanResult r = scan(options(), HeldPathsProbe({}, false));
    QVERIFY(r.projects.isEmpty());
    QVERIFY(r.probeUnavailable);
}

void TestDevArtifactScanner::scan_monorepoPackageGuardedByRepoRoot()
{
    const QDateTime old = QDateTime::currentDateTime().addDays(-60);
    const QString repo = mWork + "/mono";
    writeFile(repo + "/.git/HEAD", "ref: refs/heads/main\n");
    setMtime(repo + "/.git/HEAD", old);
    writeFile(repo + "/packages/a/package.json");
    writeFile(repo + "/packages/a/node_modules/dep/index.js");
    setMtime(repo + "/packages/a/package.json", old);

    QCOMPARE(scan(options(), nothingHeld()).projects.size(), 1);

    // A dev server started at the repo root holds packages/a/node_modules.
    const ScanResult busy = scan(options(), HeldPathsProbe({repo}, true));
    QVERIFY(busy.projects.isEmpty());
    QCOMPARE(busy.skippedInUse, 1);
}

void TestDevArtifactScanner::heldPathsProbe_matchesDirectoryAndDescendantsOnly()
{
    const HeldPathsProbe probe({"/w/proj-other/x", "/w/proj/src/a.c", "/w/zzz"}, true);
    QVERIFY(probe.holdsPathUnder("/w/proj"));
    QVERIFY(probe.holdsPathUnder("/w/proj/src"));
    QVERIFY(!probe.holdsPathUnder("/w/pro"));
    QVERIFY(!probe.holdsPathUnder("/w/proj/other"));
    QVERIFY(HeldPathsProbe({"/w/proj"}, true).holdsPathUnder("/w/proj"));
}

#if defined(Q_OS_LINUX) || defined(Q_OS_MAC)   // /proc on Linux, lsof on macOS
void TestDevArtifactScanner::fromSystem_seesProcessWithCwdInsideProject()
{
    const QString root = makeNodeProject("running", QDateTime::currentDateTime().addDays(-60));
    const QString other = makeNodeProject("quiet", QDateTime::currentDateTime().addDays(-60));

    QProcess sleeper;
    sleeper.setWorkingDirectory(root + "/src");
    sleeper.start("sleep", {"30"});
    QVERIFY(sleeper.waitForStarted(5000));

    const HeldPathsProbe probe = HeldPathsProbe::fromSystem();
    QVERIFY(probe.available());
    QVERIFY(probe.holdsPathUnder(root));
    QVERIFY(!probe.holdsPathUnder(other));

    const ScanResult r = scan(options(), probe);
    QCOMPARE(r.projects.size(), 1);
    QCOMPARE(r.projects.first().root, other);

    sleeper.kill();
    sleeper.waitForFinished(5000);
    QVERIFY(!HeldPathsProbe::fromSystem().holdsPathUnder(root));
}
#endif

// ── Git operation ────────────────────────────────────────────────────────────

void TestDevArtifactScanner::scan_gitOperationInProgressIsNotListed_data()
{
    QTest::addColumn<QString>("marker");
    QTest::addColumn<bool>("isDir");
    QTest::newRow("index.lock") << ".git/index.lock" << false;
    QTest::newRow("merge") << ".git/MERGE_HEAD" << false;
    QTest::newRow("cherry-pick") << ".git/CHERRY_PICK_HEAD" << false;
    QTest::newRow("revert") << ".git/REVERT_HEAD" << false;
    QTest::newRow("rebase-merge") << ".git/rebase-merge" << true;
    QTest::newRow("rebase-apply") << ".git/rebase-apply" << true;
    QTest::newRow("bisect") << ".git/BISECT_LOG" << false;
}

void TestDevArtifactScanner::scan_gitOperationInProgressIsNotListed()
{
    QFETCH(QString, marker);
    QFETCH(bool, isDir);
    const QDateTime old = QDateTime::currentDateTime().addDays(-60);
    const QString root = makeNodeProject("repo", old);
    writeFile(root + "/.git/HEAD", "ref: refs/heads/main\n");
    setMtime(root + "/.git/HEAD", old);

    QCOMPARE(scan(options(), nothingHeld()).projects.size(), 1);
    QVERIFY(!gitOperationInProgress(root));

    if (isDir)
        QVERIFY(QDir().mkpath(root + "/" + marker));
    else
        writeFile(root + "/" + marker);

    QVERIFY(gitOperationInProgress(root));
    const ScanResult r = scan(options(), nothingHeld());
    QVERIFY(r.projects.isEmpty());
    QCOMPARE(r.skippedGitOperation, 1);
}

void TestDevArtifactScanner::scan_linkedWorktreeGitFileIsHonoured()
{
    const QDateTime old = QDateTime::currentDateTime().addDays(-60);
    const QString root = makeNodeProject("wt", old);
    const QString gitdir = mBase + "/main/.git/worktrees/wt";
    writeFile(gitdir + "/HEAD", "ref: refs/heads/feature\n");
    writeFile(root + "/.git", ("gitdir: " + gitdir + "\n").toUtf8());
    setMtime(gitdir + "/HEAD", old);

    QCOMPARE(scan(options(), nothingHeld()).projects.size(), 1);
    writeFile(gitdir + "/index.lock");
    QCOMPARE(scan(options(), nothingHeld()).skippedGitOperation, 1);
}

// ── $HOME / deny-list / symlinks ─────────────────────────────────────────────

void TestDevArtifactScanner::rootRefusalReason_refusesHomeAndAncestorsAndRoot()
{
    QVERIFY(!rootRefusalReason(mHome, mHome).isEmpty());          // $HOME itself
    QVERIFY(!rootRefusalReason(mBase, mHome).isEmpty());          // contains $HOME
    QVERIFY(!rootRefusalReason("/", mHome).isEmpty());
    QVERIFY(!rootRefusalReason(QString(), mHome).isEmpty());
    QVERIFY(!rootRefusalReason(mBase + "/missing", mHome).isEmpty());
    QVERIFY(rootRefusalReason(mWork, mHome).isEmpty());
    QVERIFY(rootRefusalReason(mHome + "/code", mHome).isEmpty() == false);   // not created yet
    QVERIFY(QDir().mkpath(mHome + "/code"));
    QVERIFY(rootRefusalReason(mHome + "/code", mHome).isEmpty());
}

void TestDevArtifactScanner::scan_homeAsRootIsRefused()
{
    writeFile(mHome + "/code/app/package.json");
    writeFile(mHome + "/code/app/node_modules/dep/index.js");

    ScanOptions o = options();
    o.roots = {mHome};
    const ScanResult r = scan(o, nothingHeld());
    QVERIFY(r.projects.isEmpty());
    QCOMPARE(r.refusedRoots.size(), 1);
}

void TestDevArtifactScanner::scan_markerInHomeNeverMakesHomeAProject()
{
    // The scan root is an ancestor of $HOME, so the walk reaches $HOME itself.
    writeFile(mHome + "/package.json");
    writeFile(mHome + "/node_modules/dep/index.js");
    setMtime(mHome + "/package.json", QDateTime::currentDateTime().addDays(-90));

    ScanOptions o = options();
    o.roots = {mBase};                                      // refused: contains $HOME
    QVERIFY(scan(o, nothingHeld()).projects.isEmpty());

    // And a project whose root is $HOME is refused even if a root slips past.
    QVERIFY(!rootRefusalReason(mHome, mHome).isEmpty());
}

void TestDevArtifactScanner::scan_symlinkRootIntoDeniedLocationIsRefused()
{
    const QString link = mBase + "/usr-link";
    QVERIFY(QFile::link("/usr", link));

    ScanOptions o = options();
    o.roots = {link};
    const ScanResult r = scan(o, nothingHeld());
    QVERIFY(r.projects.isEmpty());
    QCOMPARE(r.refusedRoots.size(), 1);
}

void TestDevArtifactScanner::scan_symlinkRootIsResolvedBeforeJudging()
{
    // A symlink that points at $HOME is $HOME.
    const QString link = mWork + "/home-link";
    QVERIFY(QFile::link(mHome, link));
    QVERIFY(!rootRefusalReason(link, mHome).isEmpty());

    // A symlink to an ordinary project folder resolves to that folder.
    const QString root = makeNodeProject("real", QDateTime::currentDateTime().addDays(-60));
    const QString alias = mBase + "/alias";
    QVERIFY(QFile::link(mWork, alias));
    ScanOptions o = options();
    o.roots = {alias};
    const ScanResult r = scan(o, nothingHeld());
    QCOMPARE(r.projects.size(), 1);
    QCOMPARE(r.projects.first().root, root);
}

// ── Trash ────────────────────────────────────────────────────────────────────

namespace {
struct Mover {
    QStringList moved;
    bool succeed = true;
    std::function<bool(const QString &)> fn()
    {
        return [this](const QString &p) { moved.append(p); return succeed; };
    }
};
} // namespace

void TestDevArtifactScanner::trash_interactiveMovesResolvedFolder()
{
    const QString root = makeNodeProject("app", QDateTime::currentDateTime().addDays(-60));
    const Project p = scan(options(), nothingHeld()).projects.value(0);
    QCOMPARE(p.artifacts.size(), 1);

    Mover mover;
    const auto out = trashArtifacts({{p.root, p.artifacts.first().path, p.lastActivity}},
                                    RunContext::Interactive, nothingHeld(), mover.fn(), {}, mHome);
    QCOMPARE(out.size(), 1);
    QVERIFY2(out.first().trashed, qPrintable(out.first().reason));
    QCOMPARE(mover.moved, QStringList{root + "/node_modules"});
}

void TestDevArtifactScanner::trash_scheduledAndHeadlessAreRefused_data()
{
    QTest::addColumn<int>("context");
    QTest::newRow("scheduled") << int(RunContext::Scheduled);
    QTest::newRow("headless") << int(RunContext::Headless);
}

void TestDevArtifactScanner::trash_scheduledAndHeadlessAreRefused()
{
    QFETCH(int, context);
    makeNodeProject("app", QDateTime::currentDateTime().addDays(-60));
    const Project p = scan(options(), nothingHeld()).projects.value(0);

    Mover mover;
    const auto out = trashArtifacts({{p.root, p.artifacts.first().path, p.lastActivity}},
                                    RunContext(context), nothingHeld(), mover.fn(), {}, mHome);
    QVERIFY(!out.first().trashed);
    QVERIFY(mover.moved.isEmpty());
}

void TestDevArtifactScanner::trash_refusesWhenProcessAppearedSinceScan()
{
    const QString root = makeNodeProject("app", QDateTime::currentDateTime().addDays(-60));
    const Project p = scan(options(), nothingHeld()).projects.value(0);

    Mover mover;
    const auto out = trashArtifacts({{p.root, p.artifacts.first().path, p.lastActivity}},
                                    RunContext::Interactive, HeldPathsProbe({root + "/src"}, true),
                                    mover.fn(), {}, mHome);
    QVERIFY(!out.first().trashed);
    QVERIFY(out.first().reason.contains("process"));
    QVERIFY(mover.moved.isEmpty());
}

void TestDevArtifactScanner::trash_refusesWhenProjectChangedSinceScan()
{
    const QString root = makeNodeProject("app", QDateTime::currentDateTime().addDays(-60));
    const Project p = scan(options(), nothingHeld()).projects.value(0);
    writeFile(root + "/src/new.js");                         // edited after the scan

    Mover mover;
    const auto out = trashArtifacts({{p.root, p.artifacts.first().path, p.lastActivity}},
                                    RunContext::Interactive, nothingHeld(), mover.fn(), {}, mHome);
    QVERIFY(!out.first().trashed);
    QVERIFY(out.first().reason.contains("changed"));
    QVERIFY(mover.moved.isEmpty());
}

void TestDevArtifactScanner::trash_refusesNonArtifactFolder()
{
    const QString root = makeNodeProject("app", QDateTime::currentDateTime().addDays(-60));
    Mover mover;
    const QDateTime now = QDateTime::currentDateTime();
    const auto out = trashArtifacts(
        {{root, root + "/src", now},                          // source, not an artifact
         {root, root, now},                                   // the project itself
         {root, mWork, now},                                  // an ancestor
         {root, mBase + "/missing", now}},
        RunContext::Interactive, nothingHeld(), mover.fn(), {}, mHome);
    QCOMPARE(out.size(), 4);
    for (const TrashOutcome &o : out)
        QVERIFY2(!o.trashed, qPrintable(o.request.artifactPath));
    QVERIFY(mover.moved.isEmpty());
}

void TestDevArtifactScanner::trash_refusesSymlinkedArtifact()
{
    const QString root = mWork + "/linked";
    writeFile(root + "/package.json");
    writeFile(mBase + "/elsewhere/dep/index.js");
    QVERIFY(QFile::link(mBase + "/elsewhere", root + "/node_modules"));

    Mover mover;
    const auto out = trashArtifacts({{root, root + "/node_modules", QDateTime::currentDateTime()}},
                                    RunContext::Interactive, nothingHeld(), mover.fn(), {}, mHome);
    QVERIFY(!out.first().trashed);
    QVERIFY(mover.moved.isEmpty());
    QVERIFY(QFileInfo::exists(mBase + "/elsewhere/dep/index.js"));
}

void TestDevArtifactScanner::trash_refusesGitOperationAndExclusionAndHome()
{
    const QString root = makeNodeProject("app", QDateTime::currentDateTime().addDays(-60));
    writeFile(root + "/.git/HEAD", "ref: refs/heads/main\n");
    setMtime(root + "/.git/HEAD", QDateTime::currentDateTime().addDays(-60));
    const Project p = scan(options(), nothingHeld()).projects.value(0);
    const TrashRequest req{p.root, p.artifacts.first().path, p.lastActivity};

    Mover mover;
    // Exclusion list.
    auto out = trashArtifacts({req}, RunContext::Interactive, nothingHeld(), mover.fn(),
                              [&](const QString &path) { return path.startsWith(root); }, mHome);
    QVERIFY(!out.first().trashed);

    // Git operation started after the scan.
    writeFile(root + "/.git/index.lock");
    out = trashArtifacts({req}, RunContext::Interactive, nothingHeld(), mover.fn(), {}, mHome);
    QVERIFY(!out.first().trashed);
    QVERIFY(out.first().reason.contains("git"));
    QFile::remove(root + "/.git/index.lock");

    // The project root is now $HOME.
    out = trashArtifacts({req}, RunContext::Interactive, nothingHeld(), mover.fn(), {}, root);
    QVERIFY(!out.first().trashed);
    QVERIFY(mover.moved.isEmpty());
}

void TestDevArtifactScanner::trash_refusesWhenProbeUnavailable()
{
    const QString root = makeNodeProject("app", QDateTime::currentDateTime().addDays(-60));
    Mover mover;
    const auto out = trashArtifacts({{root, root + "/node_modules", QDateTime()}},
                                    RunContext::Interactive, HeldPathsProbe({}, false),
                                    mover.fn(), {}, mHome);
    QVERIFY(!out.first().trashed);
    QVERIFY(mover.moved.isEmpty());
}

void TestDevArtifactScanner::trash_failedMoveIsReported()
{
    const QString root = makeNodeProject("app", QDateTime::currentDateTime().addDays(-60));
    const Project p = scan(options(), nothingHeld()).projects.value(0);

    Mover mover;
    mover.succeed = false;
    const auto out = trashArtifacts({{p.root, p.artifacts.first().path, p.lastActivity}},
                                    RunContext::Interactive, nothingHeld(), mover.fn(), {}, mHome);
    QVERIFY(!out.first().trashed);
    QVERIFY(out.first().reason.contains("Trash"));
    QVERIFY(QFileInfo::exists(root + "/node_modules"));
}

QTEST_MAIN(TestDevArtifactScanner)
#include "test_dev_artifact_scanner.moc"
