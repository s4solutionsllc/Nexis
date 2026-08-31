// SSO-23867: unit tests for SnapshotManagerWidget::parseListLocalSnapshots()
// — pure parsing of `tmutil listlocalsnapshots /` output. No live filesystem
// or macOS system required.

#include <QtTest>
#include <Pages/Helpers/snapshot_manager_widget.h>

class TestSnapshotManagerWidget : public QObject
{
    Q_OBJECT

private slots:
    void parse_emptyOutput_returnsEmptyList()
    {
        QVERIFY(SnapshotManagerWidget::parseListLocalSnapshots(QString()).isEmpty());
        QVERIFY(SnapshotManagerWidget::parseListLocalSnapshots(QStringLiteral("\n\n")).isEmpty());
    }

    void parse_withHeaderLine_skipsHeaderAndParsesSnapshots()
    {
        const QString output = QStringLiteral(
            "Snapshots for disk /:\n"
            "com.apple.TimeMachine.2024-01-15-120000.local\n"
            "com.apple.TimeMachine.2024-01-16-093000.local\n");

        const auto entries = SnapshotManagerWidget::parseListLocalSnapshots(output);
        QCOMPARE(entries.size(), 2);
    }

    void parse_withoutHeaderLine_parsesSnapshots()
    {
        const QString output = QStringLiteral(
            "com.apple.TimeMachine.2024-01-15-120000.local\n"
            "com.apple.TimeMachine.2024-01-16-093000.local\n");

        const auto entries = SnapshotManagerWidget::parseListLocalSnapshots(output);
        QCOMPARE(entries.size(), 2);
    }

    void parse_dateTokenExtractedCorrectly()
    {
        const QString output = QStringLiteral("com.apple.TimeMachine.2024-01-15-120000.local\n");
        const auto entries = SnapshotManagerWidget::parseListLocalSnapshots(output);
        QCOMPARE(entries.size(), 1);
        QCOMPARE(entries[0].dateToken, QStringLiteral("2024-01-15-120000"));
    }

    void parse_timestampParsedAndValid()
    {
        const QString output = QStringLiteral("com.apple.TimeMachine.2024-01-15-120000.local\n");
        const auto entries = SnapshotManagerWidget::parseListLocalSnapshots(output);
        QCOMPARE(entries.size(), 1);
        QVERIFY(entries[0].timestamp.isValid());
        QCOMPARE(entries[0].timestamp.date(), QDate(2024, 1, 15));
        QCOMPARE(entries[0].timestamp.time(), QTime(12, 0, 0));
    }

    void parse_sortsNewestFirst()
    {
        const QString output = QStringLiteral(
            "com.apple.TimeMachine.2024-01-15-120000.local\n"
            "com.apple.TimeMachine.2024-01-17-080000.local\n"
            "com.apple.TimeMachine.2024-01-16-093000.local\n");

        const auto entries = SnapshotManagerWidget::parseListLocalSnapshots(output);
        QCOMPARE(entries.size(), 3);
        QCOMPARE(entries[0].dateToken, QStringLiteral("2024-01-17-080000"));
        QCOMPARE(entries[1].dateToken, QStringLiteral("2024-01-16-093000"));
        QCOMPARE(entries[2].dateToken, QStringLiteral("2024-01-15-120000"));
    }

    void parse_ignoresMalformedLinesAndBlankLines()
    {
        const QString output = QStringLiteral(
            "Snapshots for volume group containing disk3s5s1:\n"
            "not a snapshot line\n"
            "com.apple.TimeMachine.2024-01-15-120000.local\n"
            "\n"
            "   \n");

        const auto entries = SnapshotManagerWidget::parseListLocalSnapshots(output);
        QCOMPARE(entries.size(), 1);
        QCOMPARE(entries[0].dateToken, QStringLiteral("2024-01-15-120000"));
    }

    void parse_toleratesMissingLocalSuffix()
    {
        // Real tmutil output always carries the ".local" suffix, but the
        // parser shouldn't depend on it — defensive against future tmutil
        // output changes.
        const QString output = QStringLiteral("com.apple.TimeMachine.2024-01-15-120000\n");
        const auto entries = SnapshotManagerWidget::parseListLocalSnapshots(output);
        QCOMPARE(entries.size(), 1);
        QCOMPARE(entries[0].dateToken, QStringLiteral("2024-01-15-120000"));
    }
};

QTEST_MAIN(TestSnapshotManagerWidget)
#include "test_snapshot_manager_widget.moc"
