#include <QtTest>
#include <QCheckBox>
#include <QGridLayout>
#include <QStandardPaths>
#include <QToolButton>

#include "Pages/Resources/resources_page.h"
#include "Pages/Resources/history_chart.h"

// SSO-26775 stage 2: expanding a Resources card used to just hide its
// siblings, leaving the expanded card stuck in its original single grid
// cell instead of growing into the space that freed up. ResourcesPage now
// owns the relayout — these tests drive the real checkbox/grid rather than
// calling the relayout methods directly, so they fail the same way a
// regression in the wiring would.
//
// ResourcesPage is parented to an unshown-size `host` widget rather than
// shown as a top-level window: a shown top-level widget's requested
// resize() is handed to the native window system, which clamps it to the
// screen's bounds. GitHub's hosted macOS runners have no attached display,
// so their effective screen is only 1024px wide — a bare `ResourcesPage
// page; page.resize(1400, ...); page.show();` silently stayed at 1024px
// forever (not a transient settle) and never reached the 2-column
// threshold. A non-top-level child's resize() is never screen-clamped.

namespace {

QCheckBox *expandCheckbox(QWidget *card)
{
    return card->findChild<QCheckBox *>(QStringLiteral("checkHistoryTitle"));
}

struct GridSpan {
    int row = -1, col = -1, rowSpan = -1, colSpan = -1;
};

GridSpan spanOf(QGridLayout *grid, QWidget *card)
{
    GridSpan s;
    const int idx = grid->indexOf(card);
    if (idx >= 0)
        grid->getItemPosition(idx, &s.row, &s.col, &s.rowSpan, &s.colSpan);
    return s;
}

}

class TestResourcesCardExpand : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
    }

    void expand_hidesSiblingsAndSpansAllColumns_collapseRestoresLayout()
    {
        QWidget host;
        ResourcesPage page(&host);
        page.resize(1400, 900); // SSO-26719's kTwoColumnThreshold (1100) -> 2 cols
        host.show();
        page.show();
        QVERIFY(QTest::qWaitForWindowExposed(&host));
        QCOMPARE(page.width(), 1400);

        auto *cardGrid = page.findChild<QWidget *>(QStringLiteral("resourcesCardGrid"));
        QVERIFY(cardGrid);
        auto *grid = qobject_cast<QGridLayout *>(cardGrid->layout());
        QVERIFY(grid);

        const QList<HistoryChart *> cards =
            cardGrid->findChildren<HistoryChart *>(QString(), Qt::FindDirectChildrenOnly);
        QVERIFY(cards.size() >= 2);

        QHash<HistoryChart *, GridSpan> originalSpans;
        for (HistoryChart *card : cards)
            originalSpans[card] = spanOf(grid, card);
        const int originalRow0Stretch = grid->rowStretch(0);

        HistoryChart *target = cards.first();
        QCheckBox *checkbox = expandCheckbox(target);
        QVERIFY(checkbox);

        checkbox->click(); // user-driven: exercises HistoryChart::expandToggled()
        QVERIFY(checkbox->isChecked());

        for (HistoryChart *card : cards)
            QCOMPARE(card->isVisible(), card == target);

        const GridSpan expandedSpan = spanOf(grid, target);
        QCOMPARE(expandedSpan.row, 0);
        QCOMPARE(expandedSpan.col, 0);
        QCOMPARE(expandedSpan.colSpan, 2); // fills both columns freed up by its siblings

        checkbox->click(); // collapse
        QVERIFY(!checkbox->isChecked());

        for (HistoryChart *card : cards)
            QVERIFY(card->isVisible());
        for (HistoryChart *card : cards)
            QCOMPARE(spanOf(grid, card).row, originalSpans[card].row);
        for (HistoryChart *card : cards)
            QCOMPARE(spanOf(grid, card).col, originalSpans[card].col);
        // SSO-26775: placeExpandedCard() sets row 0's stretch directly; if
        // collapse doesn't clear it, row 0 keeps stretch 1 forever and grows
        // past its original height on every subsequent collapse.
        QCOMPARE(grid->rowStretch(0), originalRow0Stretch);
    }

    void expand_thenNarrowResize_staysExpandedAndSpansSingleColumn()
    {
        QWidget host;
        ResourcesPage page(&host);
        page.resize(1400, 900); // 2 cols
        host.show();
        page.show();
        QVERIFY(QTest::qWaitForWindowExposed(&host));
        QCOMPARE(page.width(), 1400);

        auto *cardGrid = page.findChild<QWidget *>(QStringLiteral("resourcesCardGrid"));
        auto *grid = qobject_cast<QGridLayout *>(cardGrid->layout());
        const QList<HistoryChart *> cards =
            cardGrid->findChildren<HistoryChart *>(QString(), Qt::FindDirectChildrenOnly);
        QVERIFY(!cards.isEmpty());

        HistoryChart *target = cards.first();
        QCheckBox *checkbox = expandCheckbox(target);
        checkbox->click();
        QVERIFY(checkbox->isChecked());

        page.resize(700, 900); // below kTwoColumnThreshold -> 1 col
        QCOMPARE(page.width(), 700);

        // A resize must not silently drop the expanded state — the card
        // stays the only visible one, still fullscreened in its new span.
        QVERIFY(checkbox->isChecked());
        for (HistoryChart *card : cards)
            QCOMPARE(card->isVisible(), card == target);

        const GridSpan span = spanOf(grid, target);
        QCOMPARE(span.row, 0);
        QCOMPARE(span.col, 0);
        QCOMPARE(span.colSpan, 1);
    }

    void liveHistorySwitch_whileExpanded_collapsesAndLeavesNoCardHidden()
    {
        QWidget host;
        ResourcesPage page(&host);
        page.resize(1400, 900);
        host.show();
        page.show();
        QVERIFY(QTest::qWaitForWindowExposed(&host));
        QCOMPARE(page.width(), 1400);

        auto *cardGrid = page.findChild<QWidget *>(QStringLiteral("resourcesCardGrid"));
        const QList<HistoryChart *> cards =
            cardGrid->findChildren<HistoryChart *>(QString(), Qt::FindDirectChildrenOnly);
        QVERIFY(!cards.isEmpty());

        QCheckBox *checkbox = expandCheckbox(cards.first());
        checkbox->click();
        QVERIFY(checkbox->isChecked());

        auto *btnHistory = page.findChild<QToolButton *>(QStringLiteral("resourcesHistoryModeButton"));
        auto *btnLive = page.findChild<QToolButton *>(QStringLiteral("resourcesLiveModeButton"));
        QVERIFY(btnHistory);
        QVERIFY(btnLive);

        btnHistory->click();
        btnLive->click();

        QVERIFY(!checkbox->isChecked());
        for (HistoryChart *card : cards)
            QVERIFY(card->isVisible());
    }
};

QTEST_MAIN(TestResourcesCardExpand)
#include "test_resources_card_expand.moc"
