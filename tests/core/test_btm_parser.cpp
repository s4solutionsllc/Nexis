// SSO-3738 / FW-10: fixture tests for the sfltool dumpbtm parser. The parser
// source is compiled directly into this test target (FR-127 pattern), so the
// tests run on any platform — even though the live `sfltool` invocation is
// only wired into the real nexis-core build on macOS.

#include <QFile>
#include <QSet>
#include <QString>
#include <QTest>

#include "btm_parser.h"

class TestBtmParser : public QObject
{
    Q_OBJECT

private:
    QByteArray loadFixture(const QString &relPath) const
    {
        const QString path = QStringLiteral(PROJECT_SOURCE_DIR)
                             + "/tests/fixtures/macos/btm/" + relPath;
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly))
            return {};
        return f.readAll();
    }

    // Test predicate: only the canonical /Applications paths and the
    // /Library/* paths exist; /tmp paths do not.
    static bool fakeExists(const QString &path)
    {
        static const QSet<QString> kExisting = {
            QStringLiteral("/Applications/Slack.app/Contents/MacOS/Slack"),
            QStringLiteral("/Applications/Dropbox.app/Contents/MacOS/Dropbox"),
            QStringLiteral("/Library/PrivilegedHelperTools/com.docker.vmnetd"),
            QStringLiteral("/Library/LaunchDaemons/com.docker.vmnetd.plist"),
        };
        return kExisting.contains(path);
    }

private slots:
    void parse_emptyInputYieldsEmptyList();
    void parse_malformedInputYieldsEmptyList();

    void parse_extractsAllRecords();
    void parse_bindsRecordsToUserSection();
    void parse_dispositionDecodedToFlagsAndBits();
    void parse_enabledDerivedFromDisposition();
    void parse_typeTokenMappedToEnum();

    void flagDuplicates_bySharedIdentifier();
    void flagDuplicates_byExecutablePath();
    void flagDuplicates_singletonsNotFlagged();

    void flagOrphans_missingExecutableTagged();
    void flagOrphans_existingExecutableNotTagged();
    void flagOrphans_applePrivatePathSkipped();

    void launchAngels_taggedAppleManaged();
    void applePrivatePath_recognizesKnownDirs();
};

// ── Edge cases ───────────────────────────────────────────────────────────────

void TestBtmParser::parse_emptyInputYieldsEmptyList()
{
    QCOMPARE(BtmParser::parse(QByteArray()).size(), 0);
}

void TestBtmParser::parse_malformedInputYieldsEmptyList()
{
    QCOMPARE(BtmParser::parse(QByteArray("not the dumpbtm output")).size(), 0);
}

// ── Parse correctness ────────────────────────────────────────────────────────

void TestBtmParser::parse_extractsAllRecords()
{
    const QByteArray content = loadFixture(QStringLiteral("dumpbtm_sample.txt"));
    QVERIFY(!content.isEmpty());

    QList<BtmRecord> records = BtmParser::parse(content);
    QCOMPARE(records.size(), 6);

    QCOMPARE(records.at(0).recordNumber, 1);
    QCOMPARE(records.at(0).name, QStringLiteral("Slack"));
    QCOMPARE(records.at(0).identifier, QStringLiteral("com.tinyspeck.slackmacgap"));
    QCOMPARE(records.at(0).teamIdentifier, QStringLiteral("BQR82RBBHL"));
    QCOMPARE(records.at(0).typeRaw, QStringLiteral("legacyLoginItem"));
    QCOMPARE(records.at(0).executablePath,
             QStringLiteral("/Applications/Slack.app/Contents/MacOS/Slack"));
    QCOMPARE(records.at(0).generation, 3);
    QCOMPARE(records.at(0).container, QStringLiteral("com.tinyspeck.slackmacgap"));

    QCOMPARE(records.at(2).plistPath,
             QStringLiteral("/Library/LaunchDaemons/com.docker.vmnetd.plist"));
    QCOMPARE(records.at(2).urlString,
             QStringLiteral("file:///Library/LaunchDaemons/com.docker.vmnetd.plist"));
}

void TestBtmParser::parse_bindsRecordsToUserSection()
{
    const QByteArray content = loadFixture(QStringLiteral("dumpbtm_sample.txt"));
    const QList<BtmRecord> records = BtmParser::parse(content);
    QVERIFY(records.size() >= 1);
    QCOMPARE(records.at(0).userUuid,
             QStringLiteral("A1B2C3D4-1111-2222-3333-444455556666"));
}

void TestBtmParser::parse_dispositionDecodedToFlagsAndBits()
{
    const QByteArray content = loadFixture(QStringLiteral("dumpbtm_sample.txt"));
    const QList<BtmRecord> records = BtmParser::parse(content);
    QVERIFY(records.size() >= 2);

    const BtmRecord &slack = records.at(0);
    QCOMPARE(slack.dispositionBits, 11);
    QVERIFY(slack.dispositionFlags.contains(QStringLiteral("enabled")));
    QVERIFY(slack.dispositionFlags.contains(QStringLiteral("allowed")));
    QVERIFY(slack.dispositionFlags.contains(QStringLiteral("visible")));
    QVERIFY(slack.dispositionFlags.contains(QStringLiteral("notified")));

    const BtmRecord &dropbox = records.at(1);
    QCOMPARE(dropbox.dispositionBits, 10);
    QVERIFY(dropbox.dispositionFlags.contains(QStringLiteral("disabled")));
}

void TestBtmParser::parse_enabledDerivedFromDisposition()
{
    const QByteArray content = loadFixture(QStringLiteral("dumpbtm_sample.txt"));
    const QList<BtmRecord> records = BtmParser::parse(content);

    QVERIFY(records.at(0).enabled);   // Slack enabled+allowed
    QVERIFY(!records.at(1).enabled);  // Dropbox disabled+allowed
    QVERIFY(records.at(2).enabled);   // Docker daemon enabled+allowed
    QVERIFY(!records.at(4).enabled);  // duplicate Slack entry disabled
}

void TestBtmParser::parse_typeTokenMappedToEnum()
{
    const QByteArray content = loadFixture(QStringLiteral("dumpbtm_sample.txt"));
    const QList<BtmRecord> records = BtmParser::parse(content);

    QCOMPARE(records.at(0).type, BtmRecordType::LegacyLoginItem);
    QCOMPARE(records.at(1).type, BtmRecordType::LoginItem);
    QCOMPARE(records.at(2).type, BtmRecordType::LaunchdDaemon);
    QCOMPARE(records.at(3).type, BtmRecordType::LaunchdAgent);
}

// ── Duplicate detection ──────────────────────────────────────────────────────

void TestBtmParser::flagDuplicates_bySharedIdentifier()
{
    const QByteArray content = loadFixture(QStringLiteral("dumpbtm_sample.txt"));
    QList<BtmRecord> records = BtmParser::parse(content);
    BtmParser::flagDuplicates(records);

    // Records #1 and #5 both have Identifier com.tinyspeck.slackmacgap.
    QVERIFY(records.at(0).duplicateIdentifier);
    QVERIFY(records.at(4).duplicateIdentifier);
    // Dropbox / Docker / ghost / LaunchAngels are unique identifiers.
    QVERIFY(!records.at(1).duplicateIdentifier);
    QVERIFY(!records.at(2).duplicateIdentifier);
    QVERIFY(!records.at(3).duplicateIdentifier);
    QVERIFY(!records.at(5).duplicateIdentifier);
}

void TestBtmParser::flagDuplicates_byExecutablePath()
{
    const QByteArray content = loadFixture(QStringLiteral("dumpbtm_sample.txt"));
    QList<BtmRecord> records = BtmParser::parse(content);
    BtmParser::flagDuplicates(records);

    QVERIFY(records.at(0).duplicateExecutable);
    QVERIFY(records.at(4).duplicateExecutable);
    QVERIFY(!records.at(2).duplicateExecutable);
}

void TestBtmParser::flagDuplicates_singletonsNotFlagged()
{
    QList<BtmRecord> records;
    BtmRecord r;
    r.identifier = QStringLiteral("com.uniq");
    r.executablePath = QStringLiteral("/Applications/Uniq.app/Contents/MacOS/Uniq");
    records << r;
    BtmParser::flagDuplicates(records);
    QVERIFY(!records.at(0).duplicateIdentifier);
    QVERIFY(!records.at(0).duplicateExecutable);
}

// ── Orphan detection ─────────────────────────────────────────────────────────

void TestBtmParser::flagOrphans_missingExecutableTagged()
{
    const QByteArray content = loadFixture(QStringLiteral("dumpbtm_sample.txt"));
    QList<BtmRecord> records = BtmParser::parse(content);
    BtmParser::flagOrphans(records, &TestBtmParser::fakeExists);

    // Record #4 is the /tmp/ghost-installer item — neither executable nor
    // plist exists on disk → orphan.
    QVERIFY(records.at(3).orphan);
}

void TestBtmParser::flagOrphans_existingExecutableNotTagged()
{
    const QByteArray content = loadFixture(QStringLiteral("dumpbtm_sample.txt"));
    QList<BtmRecord> records = BtmParser::parse(content);
    BtmParser::flagOrphans(records, &TestBtmParser::fakeExists);

    QVERIFY(!records.at(0).orphan); // Slack
    QVERIFY(!records.at(1).orphan); // Dropbox
    QVERIFY(!records.at(2).orphan); // Docker
}

void TestBtmParser::flagOrphans_applePrivatePathSkipped()
{
    const QByteArray content = loadFixture(QStringLiteral("dumpbtm_sample.txt"));
    QList<BtmRecord> records = BtmParser::parse(content);
    // Use a predicate that says everything is missing.
    BtmParser::flagOrphans(records, [](const QString &) { return false; });

    // The LaunchAngels record (#6) is Apple-managed — orphan flag must not
    // be set even though the fake predicate says the file is missing. This
    // protects against SIP hiding system files from QFileInfo::exists.
    QVERIFY(records.at(5).appleManaged);
    QVERIFY(!records.at(5).orphan);
}

// ── LaunchAngels tolerance ───────────────────────────────────────────────────

void TestBtmParser::launchAngels_taggedAppleManaged()
{
    const QByteArray content = loadFixture(QStringLiteral("dumpbtm_sample.txt"));
    const QList<BtmRecord> records = BtmParser::parse(content);
    QVERIFY(records.size() >= 6);
    QCOMPARE(records.at(5).plistPath,
             QStringLiteral("/System/Library/LaunchAngels/com.apple.btmcompanion.plist"));
    QVERIFY(records.at(5).appleManaged);
}

void TestBtmParser::applePrivatePath_recognizesKnownDirs()
{
    QVERIFY(BtmParser::isApplePrivatePath(
        QStringLiteral("/System/Library/LaunchAngels/foo.plist")));
    QVERIFY(BtmParser::isApplePrivatePath(
        QStringLiteral("/System/Library/LaunchDaemons/foo.plist")));
    QVERIFY(BtmParser::isApplePrivatePath(
        QStringLiteral("/System/Library/LaunchAgents/foo.plist")));
    QVERIFY(BtmParser::isApplePrivatePath(QStringLiteral("/usr/libexec/foo")));
    QVERIFY(!BtmParser::isApplePrivatePath(
        QStringLiteral("/Library/LaunchDaemons/foo.plist")));
    QVERIFY(!BtmParser::isApplePrivatePath(
        QStringLiteral("/tmp/foo")));
    QVERIFY(!BtmParser::isApplePrivatePath(QString()));
}

QTEST_MAIN(TestBtmParser)
#include "test_btm_parser.moc"
