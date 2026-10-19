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

// SSO-26264: runs the fetch inline instead of on a QThreadPool worker
// thread, removing the cross-thread queued-connection hop that made
// QSignalSpy::wait() here intermittently exceed its fixed timeout under
// CPU-throttled CI runners (same bug class as SSO-17858, SSO-14443). These
// tests assert the lazy-load / re-entry logic, not the real threading.
class TestableSwappinessWidget : public SwappinessWidget
{
protected:
    bool runsAsynchronously() const override { return false; }
};

class TestSwappinessWidget : public QObject
{
    Q_OBJECT

private slots:
    void loadIfNeeded_secondCallDuringSameVisitDoesNotRefetch();
    void loadIfNeeded_reentryAfterHideRefetches();
};

void TestSwappinessWidget::loadIfNeeded_secondCallDuringSameVisitDoesNotRefetch()
{
    TestableSwappinessWidget widget;
    widget.show();
    QVERIFY(QTest::qWaitForWindowExposed(&widget));

    QSignalSpy spy(&widget, &SwappinessWidget::statusFetched);

    widget.loadIfNeeded();
    // The fetch above already ran and emitted inline (runsAsynchronously()
    // is false), so the spy may already have recorded it; wait() only
    // detects emissions that happen after it's called.
    if (spy.isEmpty())
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
    TestableSwappinessWidget widget;
    widget.show();
    QVERIFY(QTest::qWaitForWindowExposed(&widget));

    QSignalSpy spy(&widget, &SwappinessWidget::statusFetched);

    widget.loadIfNeeded();
    if (spy.isEmpty())
        QVERIFY(spy.wait(5000));
    QCOMPARE(spy.count(), 1);

    // Simulate navigating away from and back to the Helpers->Swappiness
    // tool, as HelpersPage::onSwappinessClicked() does via QStackedWidget.
    widget.hide();
    widget.show();
    QVERIFY(QTest::qWaitForWindowExposed(&widget));

    const qsizetype countBeforeReentry = spy.count();
    widget.loadIfNeeded();
    if (spy.count() == countBeforeReentry)
        QVERIFY(spy.wait(5000));
    QCOMPARE(spy.count(), 2);
}

QTEST_MAIN(TestSwappinessWidget)
#include "test_swappiness_widget.moc"
