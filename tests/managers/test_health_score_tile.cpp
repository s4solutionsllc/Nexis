// GH#493 / SSO-25711: HealthScoreTile::setQuickAction() showed mBtnAction
// but never re-ran MetricTileBase::updateFooterVisibility(), so the footer
// band it lives in stayed hidden (it starts hidden in appendFooter() since
// the tile has no content yet) and the "System Checkup" button was never
// actually reachable on screen. Drive the tile through a real show()/expose
// cycle under the offscreen QPA platform, same rationale as BtmRowTests.

#include <QtTest>
#include <QPushButton>

#include "Pages/Dashboard/health_score_tile.h"

class TestHealthScoreTile : public QObject
{
    Q_OBJECT
private slots:
    void setQuickAction_makesFooterAndButtonVisible();
};

void TestHealthScoreTile::setQuickAction_makesFooterAndButtonVisible()
{
    HealthScoreTile tile("@cpuColor");
    tile.resize(200, 160);
    tile.show();
    QVERIFY(QTest::qWaitForWindowExposed(&tile));

    QWidget *footer = tile.findChild<QWidget *>(QStringLiteral("metricTileFooter"));
    QVERIFY(footer != nullptr);
    QVERIFY(!footer->isVisible());

    bool invoked = false;
    tile.setQuickAction(QStringLiteral("System Checkup"), [&invoked]() { invoked = true; });

    auto *button = tile.findChild<QPushButton *>(QStringLiteral("metricTileAction"));
    QVERIFY(button != nullptr);
    QCOMPARE(button->text(), QStringLiteral("System Checkup"));
    QVERIFY(button->isVisible());
    QVERIFY(footer->isVisible());

    button->click();
    QVERIFY(invoked);
}

QTEST_MAIN(TestHealthScoreTile)
#include "test_health_score_tile.moc"
