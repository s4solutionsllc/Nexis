// GH#490 follow-up: HelpersPage::showEvent() used to re-trigger
// loadIfNeeded() only for the HostManage tab (stack index 0). Leaving
// Helpers via the main sidebar -- not by clicking a tab button -- while
// Swappiness is the current stack page hides and re-shows HelpersPage
// itself without ever calling onSwappinessClicked() again, so the stale
// value left by SwappinessWidget::hideEvent() was never refetched. This
// re-creates that sidebar round trip and asserts the refetch now happens.

#include <QtTest>
#include <QApplication>
#include <QSignalSpy>

#include <Pages/Helpers/helpers_page.h>
#include <Pages/Helpers/swappiness_widget.h>

class TestHelpersPageReentry : public QObject
{
    Q_OBJECT

private slots:
    void showEvent_reentryOnSwappinessTabRefetches();
};

void TestHelpersPageReentry::showEvent_reentryOnSwappinessTabRefetches()
{
    HelpersPage page;
    page.show();
    QVERIFY(QTest::qWaitForWindowExposed(&page));

    auto *swappiness = page.findChild<SwappinessWidget *>();
    QVERIFY(swappiness);

    QSignalSpy spy(swappiness, &SwappinessWidget::statusFetched);

    // Switch to the Swappiness tab the way a tab-button click would.
    QVERIFY(QMetaObject::invokeMethod(&page, "onSwappinessClicked"));
    QVERIFY(spy.wait(5000));
    QCOMPARE(spy.count(), 1);

    // Simulate leaving Helpers via the main sidebar (hides/shows the whole
    // page, not the Swappiness tab widget directly) with Swappiness still
    // the current stack page.
    page.hide();
    page.show();
    QVERIFY(QTest::qWaitForWindowExposed(&page));

    QVERIFY(spy.wait(5000));
    QCOMPARE(spy.count(), 2);
}

QTEST_MAIN(TestHelpersPageReentry)
#include "test_helpers_page_reentry.moc"
