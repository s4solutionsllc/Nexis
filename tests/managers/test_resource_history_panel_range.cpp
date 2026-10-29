#include <QTest>
#include "Pages/Resources/resource_history_panel.h"

// B13 (SSO-26720): ResourceHistoryPanel must only offer history ranges the
// configured retention can actually fill — e.g. hide "30 d" when retention
// is 7 d, rather than leaving a choice that can only render an empty chart.
class TestResourceHistoryPanelRange : public QObject
{
    Q_OBJECT

private slots:
    void retentionOneDayOffersUpToADay();
    void retentionSevenDaysHidesThirtyDays();
    void retentionThirtyDaysOffersEverything();
    void subDayRetentionStillOffersSomething();
};

void TestResourceHistoryPanelRange::retentionOneDayOffersUpToADay()
{
    const QList<qint64> ranges = ResourceHistoryPanel::availableRangeSecsForRetention(1);
    QCOMPARE(ranges, QList<qint64>({ 3600, 86400 }));
}

void TestResourceHistoryPanelRange::retentionSevenDaysHidesThirtyDays()
{
    const QList<qint64> ranges = ResourceHistoryPanel::availableRangeSecsForRetention(7);
    QCOMPARE(ranges, QList<qint64>({ 3600, 86400, 604800 }));
}

void TestResourceHistoryPanelRange::retentionThirtyDaysOffersEverything()
{
    const QList<qint64> ranges = ResourceHistoryPanel::availableRangeSecsForRetention(30);
    QCOMPARE(ranges, QList<qint64>({ 3600, 86400, 604800, 2592000 }));
}

void TestResourceHistoryPanelRange::subDayRetentionStillOffersSomething()
{
    // Not reachable via the Settings UI (1/7/30 day choices) but the
    // function must never return an empty list.
    const QList<qint64> ranges = ResourceHistoryPanel::availableRangeSecsForRetention(0);
    QCOMPARE(ranges, QList<qint64>({ 3600 }));
}

QTEST_MAIN(TestResourceHistoryPanelRange)
#include "test_resource_history_panel_range.moc"
