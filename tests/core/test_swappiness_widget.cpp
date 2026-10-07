// GH#490: SwappinessWidget::loadIfNeeded() used to fetch
// /proc/sys/vm/swappiness only once per widget lifetime (mLoaded was never
// reset), so navigating away from Helpers->Swappiness and back redisplayed
// whatever was last rendered instead of re-reading the live value. The fix
// resets mLoaded in hideEvent() so the next loadIfNeeded() on re-entry
// always re-fetches, while a second loadIfNeeded() during the SAME visit
// still skips the redundant fetch.

#include <QtTest>
#include <QApplication>
#include <QSignalSpy>

#include <Pages/Helpers/swappiness_widget.h>

class TestSwappinessWidget : public QObject
{
    Q_OBJECT

private slots:
    void loadIfNeeded_secondCallDuringSameVisitDoesNotRefetch();
    void loadIfNeeded_reentryAfterHideRefetches();
};

void TestSwappinessWidget::loadIfNeeded_secondCallDuringSameVisitDoesNotRefetch()
{
    SwappinessWidget widget;
    widget.show();
    QVERIFY(QTest::qWaitForWindowExposed(&widget));

    QSignalSpy spy(&widget, &SwappinessWidget::statusFetched);

    widget.loadIfNeeded();
    QVERIFY(spy.wait(5000));
    QCOMPARE(spy.count(), 1);

    // No hide() in between -- this is still the same visit, so the lazy
    // load must skip the redundant fetch it exists to avoid.
    widget.loadIfNeeded();
    QTest::qWait(200);
    QCOMPARE(spy.count(), 1);
}

void TestSwappinessWidget::loadIfNeeded_reentryAfterHideRefetches()
{
    SwappinessWidget widget;
    widget.show();
    QVERIFY(QTest::qWaitForWindowExposed(&widget));

    QSignalSpy spy(&widget, &SwappinessWidget::statusFetched);

    widget.loadIfNeeded();
    QVERIFY(spy.wait(5000));
    QCOMPARE(spy.count(), 1);

    // Simulate navigating away from and back to the Helpers->Swappiness
    // tool, as HelpersPage::onSwappinessClicked() does via QStackedWidget.
    widget.hide();
    widget.show();
    QVERIFY(QTest::qWaitForWindowExposed(&widget));

    widget.loadIfNeeded();
    QVERIFY(spy.wait(5000));
    QCOMPARE(spy.count(), 2);
}

QTEST_MAIN(TestSwappinessWidget)
#include "test_swappiness_widget.moc"
