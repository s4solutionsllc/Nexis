// SSO-27100: GitWorktreeTool against real fixture repositories. Covers
//   1. detection of all three row kinds (folder gone, not registered, idle)
//      and the things that must NOT be listed (fresh, locked, outside an
//      agent folder, unmounted-drive look-alike);
//   2. dirty / unpushed status and the in-use flag;
//   3. removal: registered worktrees go through `git worktree remove`
//      (never --force), unregistered folders go to the Trash with an audit
//      entry, and every refusal leaves the folder in place;
//   4. the headless-run refusal and the process-in-cwd probes.
//
// HOME and the audit log are redirected into a temp dir, and the Trash is a
// fake, so nothing touches the real home, Trash or audit trail.

#include <QDir>
#include <QFile>
#include <QProcess>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

#include <utime.h>

#include "Tools/git_worktree_tool.h"
#include "Tools/lifecycle_audit_log.h"
#include "Utils/headless_util.h"

using namespace GitWorktreeTool;

namespace {

bool git(const QString &dir, const QStringList &args)
{
    QProcess p;
    p.setWorkingDirectory(dir);
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("GIT_AUTHOR_NAME"), QStringLiteral("t"));
    env.insert(QStringLiteral("GIT_AUTHOR_EMAIL"), QStringLiteral("t@example.invalid"));
    env.insert(QStringLiteral("GIT_COMMITTER_NAME"), QStringLiteral("t"));
    env.insert(QStringLiteral("GIT_COMMITTER_EMAIL"), QStringLiteral("t@example.invalid"));
    p.setProcessEnvironment(env);
    p.start(QStringLiteral("git"), args);
    return p.waitForFinished(30000) && p.exitCode() == 0;
}

QString gitOut(const QString &dir, const QStringList &args)
{
    QProcess p;
    p.setWorkingDirectory(dir);
    p.start(QStringLiteral("git"), args);
    p.waitForFinished(30000);
    return QString::fromUtf8(p.readAllStandardOutput());
}

void touch(const QString &path, int daysAgo)
{
    const time_t t = QDateTime::currentSecsSinceEpoch() - qint64(daysAgo) * 86400;
    utimbuf times{t, t};
    utime(path.toLocal8Bit().constData(), &times);
}

void writeFile(const QString &path, const QByteArray &data = "x")
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    QVERIFY2(f.open(QIODevice::WriteOnly), qPrintable(path));
    f.write(data);
}

RemoveOptions interactive(const QString &home)
{
    RemoveOptions o;
    o.homeRoot = home;
    o.appArguments = {QStringLiteral("nexis")};
    o.appArgumentsSet = true;
    o.inUse = [](const QString &) { return false; };
    return o;
}

const Worktree *find(const QList<Worktree> &rows, const QString &path)
{
    for (const Worktree &w : rows)
        if (QDir::cleanPath(w.path) == QDir::cleanPath(path))
            return &w;
    return nullptr;
}

} // namespace

class TestGitWorktreeTool : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();

    void missingFolder_isListedAndPrunedWithGit();
    void missingFolder_whenWholeAgentFolderWasDeleted_isStillListed();
    void missingFolder_outsideKnownRootsWithMissingParent_isSkipped();
    void missingFolder_removalLeavesOtherRecordsAlone();
    void unregistered_isListedAndTrashedWithAuditEntry();
    void unregistered_submoduleStyleGitFileIsNotAWorktree();
    void idle_cleanIsRemovedWithGit();
    void idle_freshWorktreeIsNotListed();
    void idle_dirtyIsBlockedAndRefused();
    void idle_unpushedCommitsAreCountedButDoNotBlock();
    void notListed_lockedAndOutsideAgentFolders();
    void inUse_blocksListingAndRemoval();
    void headlessRun_refusesRemoval();
    void blockReason_rules();
    void procProbe_findsCwdInsidePath();
    void procProbe_seesARealProcess();
    void lsofParser_extractsNameFields();
    void isAgentWorktreePath_knownFolders();

private:
    QString makeWorktree(const QString &name, const QString &root = QString());
    ScanOptions options() const;

    QTemporaryDir mTmp;
    QString mHome;
    QString mRepo;
    int mCounter = 0;
};

void TestGitWorktreeTool::initTestCase()
{
    if (!QProcess::startDetached(QStringLiteral("git"), {QStringLiteral("--version")}))
        QSKIP("git is not installed");
    QVERIFY(mTmp.isValid());
    mHome = QFileInfo(mTmp.path()).canonicalFilePath() + QStringLiteral("/home");
    QDir().mkpath(mHome);
    qputenv("HOME", mHome.toUtf8());
    QStandardPaths::setTestModeEnabled(true);
}

void TestGitWorktreeTool::init()
{
    // A fresh repo per test keeps the cases independent.
    ++mCounter;
    mRepo = mHome + QStringLiteral("/dev/repo%1").arg(mCounter);
    QDir().mkpath(mRepo);
    QVERIFY(git(mRepo, {"init", "-q", "-b", "main"}));
    QVERIFY(git(mRepo, {"commit", "-q", "--allow-empty", "-m", "init"}));
}

QString TestGitWorktreeTool::makeWorktree(const QString &name, const QString &root)
{
    const QString base = root.isEmpty() ? mRepo + QStringLiteral("/.claude/worktrees") : root;
    const QString path = base + QLatin1Char('/') + name;
    QDir().mkpath(base);
    if (!git(mRepo, {"worktree", "add", "-q", "-b", name, path}))
        QTest::qFail("git worktree add failed", __FILE__, __LINE__);
    return path;
}

ScanOptions TestGitWorktreeTool::options() const
{
    ScanOptions o;
    o.repoSearchRoots = {mHome + QStringLiteral("/dev")};
    o.idleDays = 30;
    o.inUse = [](const QString &) { return false; };
    return o;
}

void TestGitWorktreeTool::missingFolder_isListedAndPrunedWithGit()
{
    const QString wt = makeWorktree("gone");
    QVERIFY(QDir(wt).removeRecursively());

    const QList<Worktree> rows = scan(options());
    const Worktree *row = find(rows, wt);
    QVERIFY(row);
    QCOMPARE(row->kind, Worktree::Kind::MissingFolder);
    QCOMPARE(row->branch, QStringLiteral("gone"));
    QVERIFY(!row->folderExists);
    QVERIFY(blockReason(*row).isEmpty());

    const RemoveResult r = removeWorktree(*row, interactive(mHome));
    QVERIFY2(r.ok, qPrintable(r.message));
    QVERIFY(!gitOut(mRepo, {"worktree", "list", "--porcelain"}).contains(wt));
    QVERIFY(gitOut(mRepo, {"branch", "--list", "gone"}).contains("gone"));
}

void TestGitWorktreeTool::missingFolder_whenWholeAgentFolderWasDeleted_isStillListed()
{
    const QString wt = makeWorktree("a");
    QVERIFY(QDir(mRepo + QStringLiteral("/.claude")).removeRecursively());

    QVERIFY(find(scan(options()), wt));
}

void TestGitWorktreeTool::missingFolder_outsideKnownRootsWithMissingParent_isSkipped()
{
    const QString elsewhere = QFileInfo(mTmp.path()).canonicalFilePath() + QStringLiteral("/usb%1").arg(mCounter);
    const QString wt = makeWorktree("drive", elsewhere + QStringLiteral("/.claude/worktrees"));
    QVERIFY(QDir(elsewhere).removeRecursively());

    QVERIFY(!find(scan(options()), wt));
}

void TestGitWorktreeTool::missingFolder_removalLeavesOtherRecordsAlone()
{
    const QString first = makeWorktree("one");
    const QString second = makeWorktree("two");
    QVERIFY(QDir(first).removeRecursively());
    QVERIFY(QDir(second).removeRecursively());

    const QList<Worktree> rows = scan(options());
    const Worktree *one = find(rows, first);
    QVERIFY(one && find(rows, second));
    QVERIFY(removeWorktree(*one, interactive(mHome)).ok);

    const QString listing = gitOut(mRepo, {"worktree", "list", "--porcelain"});
    QVERIFY(!listing.contains(first));
    QVERIFY(listing.contains(second));
}

void TestGitWorktreeTool::unregistered_isListedAndTrashedWithAuditEntry()
{
    const QString wt = makeWorktree("orphan");
    writeFile(wt + QStringLiteral("/notes.txt"), "keep me");
    QVERIFY(QDir(mRepo + QStringLiteral("/.git/worktrees/orphan")).removeRecursively());

    const QList<Worktree> rows = scan(options());
    const Worktree *row = find(rows, wt);
    QVERIFY(row);
    QCOMPARE(row->kind, Worktree::Kind::Unregistered);
    QVERIFY(row->folderExists);
    QCOMPARE(row->dirtyCount, -1);
    QVERIFY(row->sizeBytes > 0);
    QCOMPARE(QFileInfo(row->repoPath).canonicalFilePath(), QFileInfo(mRepo).canonicalFilePath());
    QVERIFY(blockReason(*row).isEmpty());

    const int before = LifecycleAuditLog::readAll().size();
    const QString trash = mHome + QStringLiteral("/.local/share/Trash/files");
    QDir().mkpath(trash);
    RemoveOptions ro = interactive(mHome);
    ro.moveToTrash = [&](const QString &path, QString *dest) {
        *dest = trash + QLatin1Char('/') + QFileInfo(path).fileName();
        return QDir().rename(path, *dest);
    };
    const RemoveResult r = removeWorktree(*row, ro);
    QVERIFY2(r.ok, qPrintable(r.message));
    QVERIFY(!QFileInfo::exists(wt));
    QVERIFY(QFileInfo::exists(trash + QStringLiteral("/orphan/notes.txt")));

    const auto entries = LifecycleAuditLog::readAll();
    QCOMPARE(entries.size(), before + 1);
    QCOMPARE(entries.last().action, LifecycleAuditLog::Action::MovedToTrash);
    QCOMPARE(entries.last().trashDestination, trash + QStringLiteral("/orphan"));
}

void TestGitWorktreeTool::unregistered_submoduleStyleGitFileIsNotAWorktree()
{
    const QString folder = mRepo + QStringLiteral("/.claude/worktrees/sub");
    writeFile(folder + QStringLiteral("/.git"), "gitdir: ../../../.git/modules/sub\n");

    QVERIFY(!find(scan(options()), folder));
}

void TestGitWorktreeTool::idle_cleanIsRemovedWithGit()
{
    const QString wt = makeWorktree("old");
    const QString admin = mRepo + QStringLiteral("/.git/worktrees/old");
    touch(admin + QStringLiteral("/HEAD"), 60);
    touch(admin + QStringLiteral("/logs/HEAD"), 60);
    touch(wt, 60);

    const QList<Worktree> rows = scan(options());
    const Worktree *row = find(rows, wt);
    QVERIFY(row);
    QCOMPARE(row->kind, Worktree::Kind::Idle);
    QCOMPARE(row->dirtyCount, 0);
    QCOMPARE(row->unpushedCount, -1);
    QVERIFY(blockReason(*row).isEmpty());

    const RemoveResult r = removeWorktree(*row, interactive(mHome));
    QVERIFY2(r.ok, qPrintable(r.message));
    QVERIFY(!QFileInfo::exists(wt));
    QVERIFY(!gitOut(mRepo, {"worktree", "list", "--porcelain"}).contains(wt));
}

void TestGitWorktreeTool::idle_freshWorktreeIsNotListed()
{
    const QString wt = makeWorktree("fresh");
    QVERIFY(!find(scan(options()), wt));
}

void TestGitWorktreeTool::idle_dirtyIsBlockedAndRefused()
{
    const QString wt = makeWorktree("dirty");
    writeFile(wt + QStringLiteral("/wip.txt"), "unsaved work");
    const QString admin = mRepo + QStringLiteral("/.git/worktrees/dirty");
    touch(admin + QStringLiteral("/HEAD"), 60);
    touch(admin + QStringLiteral("/logs/HEAD"), 60);
    touch(wt, 60);

    const QList<Worktree> rows = scan(options());
    const Worktree *row = find(rows, wt);
    QVERIFY(row);
    QCOMPARE(row->dirtyCount, 1);
    QVERIFY(!blockReason(*row).isEmpty());

    const RemoveResult r = removeWorktree(*row, interactive(mHome));
    QVERIFY(!r.ok);
    QVERIFY(QFileInfo::exists(wt + QStringLiteral("/wip.txt")));
}

void TestGitWorktreeTool::idle_unpushedCommitsAreCountedButDoNotBlock()
{
    const QString remote = QFileInfo(mTmp.path()).canonicalFilePath() + QStringLiteral("/remote%1.git").arg(mCounter);
    QVERIFY(git(mTmp.path(), {"init", "-q", "--bare", remote}));
    QVERIFY(git(mRepo, {"remote", "add", "origin", remote}));
    QVERIFY(git(mRepo, {"push", "-q", "origin", "main"}));

    const QString wt = makeWorktree("ahead");
    QVERIFY(git(wt, {"commit", "-q", "--allow-empty", "-m", "local only"}));
    const QString admin = mRepo + QStringLiteral("/.git/worktrees/ahead");
    touch(admin + QStringLiteral("/HEAD"), 60);
    touch(admin + QStringLiteral("/logs/HEAD"), 60);
    touch(wt, 60);

    const Worktree *row = nullptr;
    const QList<Worktree> rows = scan(options());
    row = find(rows, wt);
    QVERIFY(row);
    QCOMPARE(row->unpushedCount, 1);
    QCOMPARE(row->dirtyCount, 0);
    QVERIFY(blockReason(*row).isEmpty());

    QVERIFY(removeWorktree(*row, interactive(mHome)).ok);
    QVERIFY(gitOut(mRepo, {"branch", "--list", "ahead"}).contains("ahead"));
}

void TestGitWorktreeTool::notListed_lockedAndOutsideAgentFolders()
{
    const QString locked = makeWorktree("locked");
    QVERIFY(git(mRepo, {"worktree", "lock", locked}));
    QVERIFY(QDir(locked).removeRecursively());

    const QString mine = makeWorktree("mine", mRepo + QStringLiteral("-trees"));
    QVERIFY(QDir(mine).removeRecursively());

    const QList<Worktree> rows = scan(options());
    QVERIFY(!find(rows, locked));
    QVERIFY(!find(rows, mine));
}

void TestGitWorktreeTool::inUse_blocksListingAndRemoval()
{
    const QString wt = makeWorktree("busy");
    const QString admin = mRepo + QStringLiteral("/.git/worktrees/busy");
    touch(admin + QStringLiteral("/HEAD"), 60);
    touch(admin + QStringLiteral("/logs/HEAD"), 60);
    touch(wt, 60);

    ScanOptions o = options();
    o.inUse = [&](const QString &p) { return p == wt; };
    const QList<Worktree> rows = scan(o);
    const Worktree *row = find(rows, wt);
    QVERIFY(row);
    QVERIFY(row->inUse);
    QVERIFY(!blockReason(*row).isEmpty());

    // The probe runs again at removal time, whatever the scan said.
    Worktree stale = *row;
    stale.inUse = false;
    RemoveOptions ro = interactive(mHome);
    ro.inUse = [&](const QString &p) { return p == wt; };
    QVERIFY(!removeWorktree(stale, ro).ok);
    QVERIFY(QFileInfo::exists(wt));
}

void TestGitWorktreeTool::headlessRun_refusesRemoval()
{
    const QString wt = makeWorktree("hl");
    QVERIFY(QDir(wt).removeRecursively());
    const Worktree *row = nullptr;
    const QList<Worktree> rows = scan(options());
    row = find(rows, wt);
    QVERIFY(row);

    for (const QString &flag : {QStringLiteral("--clean"), QStringLiteral("--report"),
                                 QStringLiteral("--check-threshold")}) {
        RemoveOptions ro = interactive(mHome);
        ro.appArguments = {QStringLiteral("nexis"), flag};
        QVERIFY2(!removeWorktree(*row, ro).ok, qPrintable(flag));
    }
    QVERIFY(gitOut(mRepo, {"worktree", "list", "--porcelain"}).contains(wt));
    QVERIFY(HeadlessUtil::isHeadlessArguments({QStringLiteral("nexis"), QStringLiteral("--clean")}));
    QVERIFY(!HeadlessUtil::isHeadlessArguments({QStringLiteral("nexis")}));
}

void TestGitWorktreeTool::blockReason_rules()
{
    Worktree idle;
    idle.kind = Worktree::Kind::Idle;
    idle.dirtyCount = 0;
    QVERIFY(blockReason(idle).isEmpty());
    idle.dirtyCount = 3;
    QVERIFY(!blockReason(idle).isEmpty());
    idle.dirtyCount = -1;
    QVERIFY(!blockReason(idle).isEmpty());

    Worktree orphan;
    orphan.kind = Worktree::Kind::Unregistered;
    QVERIFY(blockReason(orphan).isEmpty());
    orphan.inUse = true;
    QVERIFY(!blockReason(orphan).isEmpty());
}

void TestGitWorktreeTool::procProbe_findsCwdInsidePath()
{
    QTemporaryDir proc;
    QTemporaryDir area;
    const QString target = QFileInfo(area.path()).canonicalFilePath() + QStringLiteral("/wt");
    QDir().mkpath(target + QStringLiteral("/src"));

    QDir().mkpath(proc.path() + QStringLiteral("/100"));
    QVERIFY(QFile::link(target + QStringLiteral("/src"), proc.path() + QStringLiteral("/100/cwd")));
    QDir().mkpath(proc.path() + QStringLiteral("/200"));
    QVERIFY(QFile::link(QStringLiteral("/"), proc.path() + QStringLiteral("/200/cwd")));
    QDir().mkpath(proc.path() + QStringLiteral("/self"));

    QVERIFY(isPathInUseProc(proc.path(), target, 1));
    // Our own process never counts.
    QVERIFY(!isPathInUseProc(proc.path(), target, 100));
    QVERIFY(!isPathInUseProc(proc.path(), target + QStringLiteral("-sibling"), 1));
    // No /proc to read: fail closed.
    QVERIFY(isPathInUseProc(proc.path() + QStringLiteral("/missing"), target, 1));
}

void TestGitWorktreeTool::procProbe_seesARealProcess()
{
#if !defined(Q_OS_LINUX) && !defined(Q_OS_MACOS)
    QSKIP("needs /proc or lsof");
#endif
    QTemporaryDir area;
    const QString dir = QFileInfo(area.path()).canonicalFilePath();
    QVERIFY(!isPathInUse(dir));

    QProcess sleeper;
    sleeper.setWorkingDirectory(dir);
    sleeper.start(QStringLiteral("sleep"), {QStringLiteral("30")});
    QVERIFY(sleeper.waitForStarted());
    QVERIFY(isPathInUse(dir));
    sleeper.kill();
    sleeper.waitForFinished();
    QVERIFY(!isPathInUse(dir));
}

void TestGitWorktreeTool::lsofParser_extractsNameFields()
{
    const QStringList paths = parseLsofCwdPaths(
        QStringLiteral("p10\nfcwd\nn/Users/a/wt\np11\nfcwd\nn/\n"));
    QCOMPARE(paths, (QStringList{QStringLiteral("/Users/a/wt"), QStringLiteral("/")}));
}

void TestGitWorktreeTool::isAgentWorktreePath_knownFolders()
{
    QVERIFY(isAgentWorktreePath("/h/p/.claude/worktrees/x"));
    QVERIFY(isAgentWorktreePath("/h/.codex/worktrees/ab12/repo"));
    QVERIFY(isAgentWorktreePath("/h/.cursor/worktrees/repo/x"));
    QVERIFY(isAgentWorktreePath("/h/p/.paperclip/worktrees/SSO-1"));
    QVERIFY(!isAgentWorktreePath("/h/p/worktrees/x"));
    QVERIFY(!isAgentWorktreePath("/h/p/.claude/worktrees"));
    QVERIFY(!isAgentWorktreePath("/h/p/.claude-code/worktrees/x"));
}

QTEST_MAIN(TestGitWorktreeTool)
#include "test_git_worktree_tool.moc"
