#include <QtTest>
#include "Pages/DiskMap/bubble_layout.h"

namespace {
std::unique_ptr<DirSizeNode> mk(const QString &name, qint64 size, bool dir)
{
    auto n = std::make_unique<DirSizeNode>();
    n->name = name; n->path = "/" + name; n->size = size; n->isDir = dir;
    return n;
}
DirSizeNode *add(DirSizeNode *p, std::unique_ptr<DirSizeNode> c)
{
    DirSizeNode *raw = c.get();
    p->children.push_back(std::move(c));
    return raw;
}
}

class TestBubbleLayout : public QObject
{
    Q_OBJECT
private slots:
    void emptyOrNullFocus_yieldsNothing();
    void bigDirectory_becomesGroupWithBubblesInside();
    void smallDirectory_collapsesToTopLevelBubble();
    void bubbles_doNotOverlap();
    void topLevelAreas_areProportionalToSize();
    void everything_liesInsideArea();
    void hitTest_prefersBubbleThenGroup();
};

void TestBubbleLayout::emptyOrNullFocus_yieldsNothing()
{
    auto r = BubbleLayout::build(nullptr, QRectF(0, 0, 800, 600));
    QVERIFY(r.groups.isEmpty() && r.bubbles.isEmpty());

    auto root = mk("root", 0, true);
    r = BubbleLayout::build(root.get(), QRectF(0, 0, 800, 600));
    QVERIFY(r.groups.isEmpty() && r.bubbles.isEmpty());
}

void TestBubbleLayout::bigDirectory_becomesGroupWithBubblesInside()
{
    auto root = mk("root", 1000, true);
    DirSizeNode *big = add(root.get(), mk("big", 900, true));
    add(big, mk("a", 600, false));
    add(big, mk("b", 300, false));
    add(root.get(), mk("file", 100, false));

    const auto r = BubbleLayout::build(root.get(), QRectF(0, 0, 800, 600));
    QCOMPARE(r.groups.size(), 1);
    QCOMPARE(r.groups[0].node, big);

    int groupIdx = 0;
    int inside = 0;
    for (const auto &b : r.bubbles) {
        if (b.groupIndex == groupIdx) {
            const qreal dist = std::hypot(b.center.x() - r.groups[0].center.x(),
                                          b.center.y() - r.groups[0].center.y());
            QVERIFY2(dist + b.radius <= r.groups[0].radius + 0.5,
                     "bubble not fully inside its group circle");
            ++inside;
        }
    }
    QCOMPARE(inside, 2);
}

void TestBubbleLayout::smallDirectory_collapsesToTopLevelBubble()
{
    auto root = mk("root", 10000, true);
    add(root.get(), mk("huge", 9990, false));
    DirSizeNode *tiny = add(root.get(), mk("tiny", 10, true));
    add(tiny, mk("x", 10, false));

    const auto r = BubbleLayout::build(root.get(), QRectF(0, 0, 800, 600));
    QVERIFY(r.groups.isEmpty());
    bool found = false;
    for (const auto &b : r.bubbles)
        if (b.node == tiny) { found = true; QCOMPARE(b.groupIndex, -1); }
    QVERIFY(found);
}

void TestBubbleLayout::bubbles_doNotOverlap()
{
    auto root = mk("root", 0, true);
    for (int i = 1; i <= 10; ++i) {
        DirSizeNode *d = add(root.get(), mk(QString("d%1").arg(i), 0, true));
        for (int j = 1; j <= 6; ++j) { add(d, mk(QString("f%1").arg(j), i * j * 50, false)); d->size += i * j * 50; }
        root->size += d->size;
    }
    const auto r = BubbleLayout::build(root.get(), QRectF(0, 0, 1000, 700));

    // Same-group bubbles must not overlap by more than 1px.
    for (int i = 0; i < r.bubbles.size(); ++i) {
        for (int j = i + 1; j < r.bubbles.size(); ++j) {
            if (r.bubbles[i].groupIndex != r.bubbles[j].groupIndex || r.bubbles[i].groupIndex < 0)
                continue;
            const qreal dist = std::hypot(r.bubbles[i].center.x() - r.bubbles[j].center.x(),
                                          r.bubbles[i].center.y() - r.bubbles[j].center.y());
            const qreal minDist = r.bubbles[i].radius + r.bubbles[j].radius;
            QVERIFY2(dist >= minDist - 1.0, "same-group bubbles overlap by more than 1px");
        }
    }

    // Top-level shapes (groups + ungrouped bubbles) must not overlap either.
    struct TopShape { QPointF c; qreal r; };
    QVector<TopShape> tops;
    for (const auto &g : r.groups) tops.append({g.center, g.radius});
    for (const auto &b : r.bubbles) if (b.groupIndex < 0) tops.append({b.center, b.radius});
    for (int i = 0; i < tops.size(); ++i) {
        for (int j = i + 1; j < tops.size(); ++j) {
            const qreal dist = std::hypot(tops[i].c.x() - tops[j].c.x(), tops[i].c.y() - tops[j].c.y());
            const qreal minDist = tops[i].r + tops[j].r;
            QVERIFY2(dist >= minDist - 1.0, "top-level shapes overlap by more than 1px");
        }
    }
}

void TestBubbleLayout::topLevelAreas_areProportionalToSize()
{
    auto root = mk("root", 300, true);
    DirSizeNode *a = add(root.get(), mk("a", 200, false));
    DirSizeNode *b = add(root.get(), mk("b", 100, false));
    const auto r = BubbleLayout::build(root.get(), QRectF(0, 0, 600, 400));

    qreal aa = 0, ab = 0;
    for (const auto &bub : r.bubbles) {
        if (bub.node == a) aa = M_PI * bub.radius * bub.radius;
        if (bub.node == b) ab = M_PI * bub.radius * bub.radius;
    }
    QVERIFY(aa > 0 && ab > 0);
    QVERIFY2(qAbs(aa / ab - 2.0) < 0.1, qPrintable(QString("ratio %1").arg(aa / ab)));
}

void TestBubbleLayout::everything_liesInsideArea()
{
    auto root = mk("root", 0, true);
    for (int i = 1; i <= 8; ++i) {
        DirSizeNode *d = add(root.get(), mk(QString("d%1").arg(i), 0, true));
        for (int j = 1; j <= 4; ++j) { add(d, mk(QString("f%1").arg(j), i * j * 30, false)); d->size += i * j * 30; }
        root->size += d->size;
    }
    const QRectF area(0, 0, 900, 600);
    const auto r = BubbleLayout::build(root.get(), area);

    for (const auto &g : r.groups) {
        QVERIFY2(area.contains(QRectF(g.center.x() - g.radius, g.center.y() - g.radius,
                                      g.radius * 2, g.radius * 2).adjusted(1, 1, -1, -1)),
                 "group extends outside area");
    }
    for (const auto &b : r.bubbles) {
        QVERIFY2(area.contains(QRectF(b.center.x() - b.radius, b.center.y() - b.radius,
                                      b.radius * 2, b.radius * 2).adjusted(1, 1, -1, -1)),
                 "bubble extends outside area");
    }
}

void TestBubbleLayout::hitTest_prefersBubbleThenGroup()
{
    auto root = mk("root", 1000, true);
    DirSizeNode *big = add(root.get(), mk("big", 1000, true));
    add(big, mk("leaf", 1000, false));
    const auto r = BubbleLayout::build(root.get(), QRectF(0, 0, 800, 600));

    QVERIFY(!r.groups.isEmpty());
    QVERIFY(!r.bubbles.isEmpty());
    QCOMPARE(BubbleLayout::hitTest(r, r.bubbles[0].center), r.bubbles[0].node);

    // A point inside the group's membrane gap but outside every bubble
    // should hit the group itself.
    const auto &g = r.groups[0];
    QPointF gapPoint = g.center + QPointF(g.radius - 1.0, 0);
    bool insideAnyBubble = false;
    for (const auto &b : r.bubbles) {
        if (std::hypot(gapPoint.x() - b.center.x(), gapPoint.y() - b.center.y()) <= b.radius)
            insideAnyBubble = true;
    }
    if (!insideAnyBubble)
        QCOMPARE(BubbleLayout::hitTest(r, gapPoint), big);

    QCOMPARE(BubbleLayout::hitTest(r, QPointF(-50, -50)), nullptr);
}

QTEST_APPLESS_MAIN(TestBubbleLayout)
#include "test_bubble_layout.moc"
