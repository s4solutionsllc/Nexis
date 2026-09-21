#include <QtTest>
#include <QElapsedTimer>
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
    void topLevelCluster_fillsWideArea();
    void topLevelShapes_touchAfterCompaction();
    void nestedPack_fillsAndCentresInMembrane();
    void largeGroup_staysFastAndValid();
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

// SSO-24963 fix round 1: the top-level cluster must fill the destination
// rect (bounding-box fit + aspect-biased packing), not float as an
// undersized disc in the middle of it.
void TestBubbleLayout::topLevelCluster_fillsWideArea()
{
    auto root = mk("root", 0, true);
    const qint64 sizes[] = {4000, 900, 600, 300, 120, 60};
    for (int i = 0; i < 6; ++i) {
        add(root.get(), mk(QString("c%1").arg(i), sizes[i], false));
        root->size += sizes[i];
    }
    const QRectF area(0, 0, 1200, 600);
    const auto r = BubbleLayout::build(root.get(), area);

    qreal minX = std::numeric_limits<qreal>::max(), maxX = std::numeric_limits<qreal>::lowest();
    qreal minY = std::numeric_limits<qreal>::max(), maxY = std::numeric_limits<qreal>::lowest();
    for (const auto &g : r.groups) {
        minX = std::min(minX, g.center.x() - g.radius); maxX = std::max(maxX, g.center.x() + g.radius);
        minY = std::min(minY, g.center.y() - g.radius); maxY = std::max(maxY, g.center.y() + g.radius);
    }
    for (const auto &b : r.bubbles) {
        if (b.groupIndex >= 0) continue;
        minX = std::min(minX, b.center.x() - b.radius); maxX = std::max(maxX, b.center.x() + b.radius);
        minY = std::min(minY, b.center.y() - b.radius); maxY = std::max(maxY, b.center.y() + b.radius);
    }

    const qreal boxW = maxX - minX, boxH = maxY - minY;
    const qreal wRatio = boxW / area.width(), hRatio = boxH / area.height();

    const bool ok = (wRatio >= 0.70 && hRatio >= 0.85) || (std::min(wRatio, hRatio) >= 0.90);
    QVERIFY2(ok, qPrintable(QString("bounding box %1x%2 in %3x%4 area (w=%5%%, h=%6%%)")
                             .arg(boxW).arg(boxH).arg(area.width()).arg(area.height())
                             .arg(wRatio * 100).arg(hRatio * 100)));
}

// SSO-24963 fix round 1: the compaction phase must close visible gaps
// between top-level neighbours.
void TestBubbleLayout::topLevelShapes_touchAfterCompaction()
{
    auto root = mk("root", 0, true);
    const qint64 sizes[] = {2000, 800, 500, 250, 100};
    for (int i = 0; i < 5; ++i) {
        add(root.get(), mk(QString("c%1").arg(i), sizes[i], false));
        root->size += sizes[i];
    }
    const auto r = BubbleLayout::build(root.get(), QRectF(0, 0, 1000, 600));

    struct TopShape { QPointF c; qreal r; };
    QVector<TopShape> tops;
    for (const auto &g : r.groups) tops.append({g.center, g.radius});
    for (const auto &b : r.bubbles) if (b.groupIndex < 0) tops.append({b.center, b.radius});
    QCOMPARE(tops.size(), 5);

    for (int i = 0; i < tops.size(); ++i) {
        qreal bestGap = std::numeric_limits<qreal>::max();
        for (int j = 0; j < tops.size(); ++j) {
            if (i == j) continue;
            const qreal dist = std::hypot(tops[i].c.x() - tops[j].c.x(), tops[i].c.y() - tops[j].c.y());
            bestGap = std::min(bestGap, dist - (tops[i].r + tops[j].r));
        }
        QVERIFY2(bestGap <= 2.0, qPrintable(QString("shape %1 is %2px from its nearest neighbour").arg(i).arg(bestGap)));
    }
}

// SSO-24963 fix round 1: the nested pack must fill and be concentric with
// the membrane's usable inner disc, not hug one side undersized.
void TestBubbleLayout::nestedPack_fillsAndCentresInMembrane()
{
    auto root = mk("root", 1000, true);
    DirSizeNode *big = add(root.get(), mk("big", 990, true));
    add(big, mk("a", 60, false));
    add(big, mk("b", 30, false));
    add(big, mk("c", 10, false));
    add(root.get(), mk("file", 10, false));

    BubbleLayout::Metrics m;
    const auto r = BubbleLayout::build(root.get(), QRectF(0, 0, 800, 600), m);
    QCOMPARE(r.groups.size(), 1);
    const auto &g = r.groups[0];

    qreal discR = g.radius - m.innerPad;
    QPointF discCenter = g.center;
    if (!BubbleLayout::labelBandRect(g, m).isEmpty()) {
        discR -= m.labelBand / 2.0;
        discCenter += QPointF(0, m.labelBand / 2.0);
    }
    QVERIFY(discR > 0);

    qreal minX = std::numeric_limits<qreal>::max(), maxX = std::numeric_limits<qreal>::lowest();
    qreal minY = std::numeric_limits<qreal>::max(), maxY = std::numeric_limits<qreal>::lowest();
    int count = 0;
    for (const auto &b : r.bubbles) {
        if (b.groupIndex != 0) continue;
        ++count;
        minX = std::min(minX, b.center.x() - b.radius); maxX = std::max(maxX, b.center.x() + b.radius);
        minY = std::min(minY, b.center.y() - b.radius); maxY = std::max(maxY, b.center.y() + b.radius);
    }
    QCOMPARE(count, 3);

    const QPointF boxCenter((minX + maxX) / 2.0, (minY + maxY) / 2.0);
    qreal enclosingR = 0;
    for (const auto &b : r.bubbles) {
        if (b.groupIndex != 0) continue;
        enclosingR = std::max(enclosingR, std::hypot(b.center.x() - boxCenter.x(), b.center.y() - boxCenter.y()) + b.radius);
    }

    QVERIFY2(enclosingR >= 0.90 * discR,
             qPrintable(QString("enclosing R %1 < 90%% of usable inner radius %2").arg(enclosingR).arg(discR)));
    const qreal centreDist = std::hypot(boxCenter.x() - discCenter.x(), boxCenter.y() - discCenter.y());
    QVERIFY2(centreDist <= 3.0, qPrintable(QString("cluster centre is %1px from the usable disc centre").arg(centreDist)));
}

// SSO-24963 fix round 1: a group with hundreds of power-law-sized children
// (capped at 150) must still satisfy every invariant and stay fast.
void TestBubbleLayout::largeGroup_staysFastAndValid()
{
    auto root = mk("root", 0, true);
    DirSizeNode *big = add(root.get(), mk("big", 0, true));
    qint64 total = 0;
    for (int i = 0; i < 250; ++i) {
        const qint64 size = std::max<qint64>(1, qint64(4'000'000.0 / std::pow(i + 1, 1.5)));
        add(big, mk(QString("f%1").arg(i), size, false));
        total += size;
    }
    big->size = total;
    root->size = total;

    QElapsedTimer timer;
    timer.start();
    const auto r = BubbleLayout::build(root.get(), QRectF(0, 0, 1200, 700));
    const qint64 elapsedMs = timer.elapsed();
    QVERIFY2(elapsedMs < 250, qPrintable(QString("build() took %1ms").arg(elapsedMs)));

    QCOMPARE(r.groups.size(), 1);
    const auto &g = r.groups[0];
    int nested = 0;
    for (const auto &b : r.bubbles) {
        if (b.groupIndex != 0) continue;
        ++nested;
        const qreal dist = std::hypot(b.center.x() - g.center.x(), b.center.y() - g.center.y());
        QVERIFY2(dist + b.radius <= g.radius + 0.5, "nested bubble escapes its membrane");
    }
    QVERIFY(nested > 0);

    for (int i = 0; i < r.bubbles.size(); ++i) {
        for (int j = i + 1; j < r.bubbles.size(); ++j) {
            if (r.bubbles[i].groupIndex != r.bubbles[j].groupIndex || r.bubbles[i].groupIndex < 0)
                continue;
            const qreal dist = std::hypot(r.bubbles[i].center.x() - r.bubbles[j].center.x(),
                                          r.bubbles[i].center.y() - r.bubbles[j].center.y());
            QVERIFY2(dist >= r.bubbles[i].radius + r.bubbles[j].radius - 1.0, "same-group bubbles overlap by more than 1px");
        }
    }
}

QTEST_APPLESS_MAIN(TestBubbleLayout)
#include "test_bubble_layout.moc"
