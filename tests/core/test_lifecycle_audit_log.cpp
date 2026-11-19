// SSO-15386 / SSO-15373 (CISO safety controls, §3): append-only audit trail
// for every lifecycle-manager deletion. Tests cover:
//   1. Round-trip: appended entries read back with all fields intact.
//   2. Append-only: writes never clobber prior entries.
//   3. File permissions are locked to the owner only (0600) after the
//      first write.
//   4. Retention: prune() keeps recent-by-age and recent-by-count entries,
//      dropping only entries that are both old and beyond the count floor.
//
// Uses QStandardPaths::setTestModeEnabled(true) (existing project pattern,
// see test_stacer_importer.cpp) so the log never touches the real user
// data directory.

#include <QTest>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QFileDevice>
#include <QFileInfo>
#include <QStandardPaths>

#include "Tools/lifecycle_audit_log.h"

using LifecycleAuditLog::Action;
using LifecycleAuditLog::Entry;

class TestLifecycleAuditLog : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();

    void append_roundTripsAllFields();
    void append_isAppendOnly_priorEntriesSurvive();
    void append_locksDownFilePermissions();
    void prune_keepsMinRetainedCountRegardlessOfAge();
    void prune_dropsOldEntriesBeyondCountFloor();

    // "Recently removed" history view: cleaner-run fields, grouping, restore.
    void append_roundTripsCleanerRunFields();
    void appendAll_writesEveryEntry();
    void groupIntoRuns_sumsBytesAndCountsSkipsNewestFirst();
    void restore_movesTrashedItemBackAndRecordsIt();
    void restore_refusesWhenDestinationExists();
    void restore_refusesWhenTrashItemIsGone();
    void restore_refusesPathsOutsideTrash();
    void restore_refusesPermanentlyDeletedEntry();

private:
    static Entry makeEntry(const QString &batchId, const QDateTime &timestamp);
};

void TestLifecycleAuditLog::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
}

void TestLifecycleAuditLog::init()
{
    QFile::remove(LifecycleAuditLog::logFilePath());
}

Entry TestLifecycleAuditLog::makeEntry(const QString &batchId, const QDateTime &timestamp)
{
    Entry e;
    e.timestamp = timestamp;
    e.batchId = batchId;
    e.originalPath = QStringLiteral("/home/user/.config/com.example.Old");
    e.canonicalPath = QStringLiteral("/home/user/.config/com.example.Old");
    e.action = Action::MovedToTrash;
    e.trashDestination = QStringLiteral("/home/user/.local/share/Trash/files/com.example.Old");
    e.matchingRuleIds = {QStringLiteral("no_installed_app"), QStringLiteral("age_threshold")};
    e.confidenceScore = 3;
    e.sizeBytes = 4096;
    e.nexisVersion = QStringLiteral("1.2.3-test");
    e.processStopAction = QString();
    return e;
}

void TestLifecycleAuditLog::append_roundTripsAllFields()
{
    const QDateTime ts = QDateTime::currentDateTimeUtc();
    const Entry written = makeEntry(QStringLiteral("batch-1"), ts);

    QVERIFY(LifecycleAuditLog::append(written));

    const QList<Entry> all = LifecycleAuditLog::readAll();
    QCOMPARE(all.size(), 1);

    const Entry &read = all.first();
    QCOMPARE(read.batchId, written.batchId);
    QCOMPARE(read.originalPath, written.originalPath);
    QCOMPARE(read.canonicalPath, written.canonicalPath);
    QCOMPARE(static_cast<int>(read.action), static_cast<int>(written.action));
    QCOMPARE(read.trashDestination, written.trashDestination);
    QCOMPARE(read.matchingRuleIds, written.matchingRuleIds);
    QCOMPARE(read.confidenceScore, written.confidenceScore);
    QCOMPARE(read.sizeBytes, written.sizeBytes);
    QCOMPARE(read.nexisVersion, written.nexisVersion);
    // Millisecond round-trip via Qt::ISODateWithMs.
    QCOMPARE(read.timestamp.toMSecsSinceEpoch(), written.timestamp.toMSecsSinceEpoch());
}

void TestLifecycleAuditLog::append_isAppendOnly_priorEntriesSurvive()
{
    const QDateTime now = QDateTime::currentDateTimeUtc();
    QVERIFY(LifecycleAuditLog::append(makeEntry(QStringLiteral("batch-1"), now.addSecs(-2))));
    QVERIFY(LifecycleAuditLog::append(makeEntry(QStringLiteral("batch-2"), now.addSecs(-1))));
    QVERIFY(LifecycleAuditLog::append(makeEntry(QStringLiteral("batch-3"), now)));

    const QList<Entry> all = LifecycleAuditLog::readAll();
    QCOMPARE(all.size(), 3);
    QCOMPARE(all.at(0).batchId, QStringLiteral("batch-1"));
    QCOMPARE(all.at(1).batchId, QStringLiteral("batch-2"));
    QCOMPARE(all.at(2).batchId, QStringLiteral("batch-3"));
}

void TestLifecycleAuditLog::append_locksDownFilePermissions()
{
    QVERIFY(LifecycleAuditLog::append(makeEntry(QStringLiteral("batch-1"), QDateTime::currentDateTimeUtc())));

#ifndef Q_OS_WIN
    const QFileDevice::Permissions perms = QFileInfo(LifecycleAuditLog::logFilePath()).permissions();
    QVERIFY(!(perms & QFileDevice::ReadGroup));
    QVERIFY(!(perms & QFileDevice::WriteGroup));
    QVERIFY(!(perms & QFileDevice::ReadOther));
    QVERIFY(!(perms & QFileDevice::WriteOther));
#endif
}

void TestLifecycleAuditLog::prune_keepsMinRetainedCountRegardlessOfAge()
{
    const QDateTime old = QDateTime::currentDateTimeUtc().addDays(-365);
    for (int i = 0; i < 5; ++i)
        QVERIFY(LifecycleAuditLog::append(makeEntry(QStringLiteral("old-%1").arg(i), old.addSecs(i))));

    // All 5 entries are far older than 90 days, but minRetainedCount=5
    // must keep every one of them (retain last N OR 90 days, whichever
    // is *longer* — not the intersection).
    LifecycleAuditLog::prune(5);

    QCOMPARE(LifecycleAuditLog::readAll().size(), 5);
}

void TestLifecycleAuditLog::prune_dropsOldEntriesBeyondCountFloor()
{
    const QDateTime old = QDateTime::currentDateTimeUtc().addDays(-365);
    for (int i = 0; i < 10; ++i)
        QVERIFY(LifecycleAuditLog::append(makeEntry(QStringLiteral("old-%1").arg(i), old.addSecs(i))));

    LifecycleAuditLog::prune(3);

    const QList<Entry> remaining = LifecycleAuditLog::readAll();
    QCOMPARE(remaining.size(), 3);
    // The 3 most recent (by append order / timestamp) survive.
    QCOMPARE(remaining.at(0).batchId, QStringLiteral("old-7"));
    QCOMPARE(remaining.at(1).batchId, QStringLiteral("old-8"));
    QCOMPARE(remaining.at(2).batchId, QStringLiteral("old-9"));
}

namespace {

Entry cleanerEntry(const QString &batchId, Action action, quint64 size, const QString &source,
                   const QDateTime &ts = QDateTime::currentDateTimeUtc())
{
    Entry e;
    e.timestamp = ts;
    e.batchId = batchId;
    e.originalPath = QStringLiteral("/home/user/Downloads/file-%1.zip").arg(size);
    e.canonicalPath = e.originalPath;
    e.action = action;
    e.sizeBytes = size;
    e.runSource = source;
    e.category = QStringLiteral("Old Downloads");
    return e;
}

// A trashed item laid out the way QFile::moveToTrash leaves it on Linux.
struct TrashedItem
{
    QString original;
    QString inTrash;
    Entry entry;
};

TrashedItem makeTrashedItem(const QTemporaryDir &root)
{
    TrashedItem item;
    item.original = root.filePath(QStringLiteral("Downloads/old.zip"));
    item.inTrash = root.filePath(QStringLiteral(".local/share/Trash/files/old.zip"));
    QDir().mkpath(QFileInfo(item.inTrash).absolutePath());
    QDir().mkpath(root.filePath(QStringLiteral(".local/share/Trash/info")));
    QFile f(item.inTrash);
    static_cast<void>(f.open(QIODevice::WriteOnly));
    f.write("payload");
    f.close();
    QFile info(root.filePath(QStringLiteral(".local/share/Trash/info/old.zip.trashinfo")));
    static_cast<void>(info.open(QIODevice::WriteOnly));
    info.write("[Trash Info]\n");
    info.close();

    item.entry = cleanerEntry(QStringLiteral("run-1"), Action::MovedToTrash, 7,
                              LifecycleAuditLog::RunSource::Interactive);
    item.entry.originalPath = item.original;
    item.entry.canonicalPath = item.original;
    item.entry.trashDestination = item.inTrash;
    return item;
}

} // namespace

void TestLifecycleAuditLog::append_roundTripsCleanerRunFields()
{
    Entry written = cleanerEntry(QStringLiteral("run-1"), Action::Skipped, 0,
                                 LifecycleAuditLog::RunSource::Headless);
    written.skipReason = QStringLiteral("excluded");
    QVERIFY(LifecycleAuditLog::append(written));

    const QList<Entry> all = LifecycleAuditLog::readAll();
    QCOMPARE(all.size(), 1);
    QCOMPARE(static_cast<int>(all.first().action), static_cast<int>(Action::Skipped));
    QCOMPARE(all.first().runSource, LifecycleAuditLog::RunSource::Headless);
    QCOMPARE(all.first().category, QStringLiteral("Old Downloads"));
    QCOMPARE(all.first().skipReason, QStringLiteral("excluded"));
}

void TestLifecycleAuditLog::appendAll_writesEveryEntry()
{
    const QDateTime now = QDateTime::currentDateTimeUtc();
    QVERIFY(LifecycleAuditLog::appendAll({
        cleanerEntry(QStringLiteral("run-1"), Action::PermanentlyDeleted, 1, QString(), now),
        cleanerEntry(QStringLiteral("run-1"), Action::PermanentlyDeleted, 2, QString(), now),
        cleanerEntry(QStringLiteral("run-1"), Action::PermanentlyDeleted, 3, QString(), now),
    }));
    QCOMPARE(LifecycleAuditLog::readAll().size(), 3);
    QVERIFY(LifecycleAuditLog::appendAll({}));
    QCOMPARE(LifecycleAuditLog::readAll().size(), 3);
}

void TestLifecycleAuditLog::groupIntoRuns_sumsBytesAndCountsSkipsNewestFirst()
{
    const QDateTime now = QDateTime::currentDateTimeUtc();
    const QString interactive = LifecycleAuditLog::RunSource::Interactive;
    const QString scheduled = LifecycleAuditLog::RunSource::Scheduled;
    const QList<Entry> entries = {
        cleanerEntry(QStringLiteral("old"), Action::PermanentlyDeleted, 100, scheduled, now.addSecs(-100)),
        cleanerEntry(QStringLiteral("new"), Action::MovedToTrash, 10, interactive, now),
        cleanerEntry(QStringLiteral("new"), Action::PermanentlyDeleted, 20, interactive, now),
        cleanerEntry(QStringLiteral("new"), Action::Skipped, 0, interactive, now),
    };

    const QList<LifecycleAuditLog::Run> runs = LifecycleAuditLog::groupIntoRuns(entries);

    QCOMPARE(runs.size(), 2);
    QCOMPARE(runs.at(0).batchId, QStringLiteral("new"));
    QCOMPARE(runs.at(0).runSource, interactive);
    QCOMPARE(runs.at(0).bytesFreed, quint64(30));
    QCOMPARE(runs.at(0).removedCount, 2);
    QCOMPARE(runs.at(0).skippedCount, 1);
    QCOMPARE(runs.at(0).items.size(), 3);
    QCOMPARE(runs.at(1).batchId, QStringLiteral("old"));
    QCOMPARE(runs.at(1).runSource, scheduled);
}

void TestLifecycleAuditLog::restore_movesTrashedItemBackAndRecordsIt()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const TrashedItem item = makeTrashedItem(root);

    const LifecycleAuditLog::RestoreResult result = LifecycleAuditLog::restore(item.entry);

    QVERIFY2(result.ok(), qPrintable(result.message));
    QVERIFY(QFile::exists(item.original));
    QVERIFY(!QFile::exists(item.inTrash));
    QVERIFY(!QFile::exists(root.filePath(QStringLiteral(".local/share/Trash/info/old.zip.trashinfo"))));

    // The Restored entry hides the Restore action for that item in the view.
    const QList<LifecycleAuditLog::Run> runs = LifecycleAuditLog::groupIntoRuns(
        QList<Entry>{item.entry} + LifecycleAuditLog::readAll());
    QCOMPARE(runs.size(), 1);
    QVERIFY(!runs.first().isRestorable(item.entry));
}

void TestLifecycleAuditLog::restore_refusesWhenDestinationExists()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const TrashedItem item = makeTrashedItem(root);
    QDir().mkpath(QFileInfo(item.original).absolutePath());
    QFile existing(item.original);
    QVERIFY(existing.open(QIODevice::WriteOnly));
    existing.write("newer");
    existing.close();

    const LifecycleAuditLog::RestoreResult result = LifecycleAuditLog::restore(item.entry);

    QCOMPARE(static_cast<int>(result.status),
             static_cast<int>(LifecycleAuditLog::RestoreResult::Status::DestinationExists));
    QVERIFY(result.message.contains(item.original));
    QVERIFY(QFile::exists(item.inTrash));
    QFile check(item.original);
    QVERIFY(check.open(QIODevice::ReadOnly));
    QCOMPARE(check.readAll(), QByteArray("newer"));
    QVERIFY(LifecycleAuditLog::readAll().isEmpty());
}

void TestLifecycleAuditLog::restore_refusesWhenTrashItemIsGone()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const TrashedItem item = makeTrashedItem(root);
    QVERIFY(QFile::remove(item.inTrash));

    const LifecycleAuditLog::RestoreResult result = LifecycleAuditLog::restore(item.entry);

    QCOMPARE(static_cast<int>(result.status),
             static_cast<int>(LifecycleAuditLog::RestoreResult::Status::TrashItemMissing));
    QVERIFY(!QFile::exists(item.original));
    QVERIFY(LifecycleAuditLog::readAll().isEmpty());
}

void TestLifecycleAuditLog::restore_refusesPathsOutsideTrash()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    TrashedItem item = makeTrashedItem(root);
    // A tampered log entry pointing at a live file must never be moved.
    const QString victim = root.filePath(QStringLiteral("documents/important.txt"));
    QDir().mkpath(QFileInfo(victim).absolutePath());
    QFile f(victim);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.close();
    item.entry.trashDestination = victim;

    const LifecycleAuditLog::RestoreResult result = LifecycleAuditLog::restore(item.entry);

    QCOMPARE(static_cast<int>(result.status),
             static_cast<int>(LifecycleAuditLog::RestoreResult::Status::NotRestorable));
    QVERIFY(QFile::exists(victim));
    QVERIFY(!QFile::exists(item.original));
}

void TestLifecycleAuditLog::restore_refusesPermanentlyDeletedEntry()
{
    Entry e = cleanerEntry(QStringLiteral("run-1"), Action::PermanentlyDeleted, 5,
                           LifecycleAuditLog::RunSource::Scheduled);
    QCOMPARE(static_cast<int>(LifecycleAuditLog::restore(e).status),
             static_cast<int>(LifecycleAuditLog::RestoreResult::Status::NotRestorable));
}

QTEST_MAIN(TestLifecycleAuditLog)
#include "test_lifecycle_audit_log.moc"
