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
#include <QFile>
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

QTEST_MAIN(TestLifecycleAuditLog)
#include "test_lifecycle_audit_log.moc"
