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
    void topLevelCluster_fillsWideArea();
    void topLevelShapes_touchAfterCompaction();
    void nestedPack_fillsAndCentresInMembrane();
    void largeGroup_staysFastAndValid();
    void packCache_matchesUncachedGeometry();
    void packCache_warmSecondBuild_noNewPacks();
    void packCache_clear_forcesRepack();
    void labelBandRect_emptyWhenChordTooNarrow_insideMembraneOtherwise();
    void packsCached_trueForTrivialOrNullCache();
    void packsCached_falseOnMissTrueOnceWarm();
    void packsCached_falseWhenOnlyNestedPackIsMissing();
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

    const auto r = BubbleLayout::build(root.get(), QRectF(0, 0, 1200, 700));

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

// SSO-24963 review round 2 (Minor 12): labelBandRect() is the single source
// of truth build() and BubbleMapView both rely on for whether a group gets
// a label — test it directly rather than only indirectly through build().
void TestBubbleLayout::labelBandRect_emptyWhenChordTooNarrow_insideMembraneOtherwise()
{
    BubbleLayout::Metrics m; // labelBand = 16 by default

    BubbleLayout::Group tooSmall;
    tooSmall.center = QPointF(100, 100);
    tooSmall.radius = 10; // <= m.labelBand
    QVERIFY(BubbleLayout::labelBandRect(tooSmall, m).isEmpty());

    BubbleLayout::Group narrowChord;
    narrowChord.center = QPointF(100, 100);
    narrowChord.radius = 17; // > labelBand, but chord at the baseline < 60px
    QVERIFY(BubbleLayout::labelBandRect(narrowChord, m).isEmpty());

    BubbleLayout::Group roomy;
    roomy.center = QPointF(300, 250);
    roomy.radius = 200;
    const QRectF band = BubbleLayout::labelBandRect(roomy, m);
    QVERIFY(!band.isEmpty());
    QVERIFY2(band.width() >= 60.0, qPrintable(QString("chord %1 < 60px").arg(band.width())));

    // The band's own width is exactly the chord at its vertical centre (the
    // baseline) — those left/right points sit precisely on the membrane's
    // rim by construction, so they're the meaningful "inside the membrane"
    // check; the band's top/bottom edges (offset by half the label height)
    // are allowed to graze slightly past the rim for a rim-hugging label.
    const QPointF leftAtBaseline(band.left(), band.center().y());
    const QPointF rightAtBaseline(band.right(), band.center().y());
    for (const auto &pt : {leftAtBaseline, rightAtBaseline}) {
        const qreal dist = std::hypot(pt.x() - roomy.center.x(), pt.y() - roomy.center.y());
        QVERIFY2(dist <= roomy.radius + 0.5,
                 qPrintable(QString("label band edge is %1px outside the membrane (radius %2)")
                                .arg(dist).arg(roomy.radius)));
    }
    // And the whole band sits within the membrane's bounding box.
    QVERIFY(QRectF(roomy.center.x() - roomy.radius, roomy.center.y() - roomy.radius,
                   roomy.radius * 2, roomy.radius * 2).contains(band.adjusted(1, 1, -1, -1)));
}

namespace {
void compareResults(const BubbleLayout::Result &a, const BubbleLayout::Result &b)
{
    QCOMPARE(a.groups.size(), b.groups.size());
    for (int i = 0; i < a.groups.size(); ++i) {
        QVERIFY(std::hypot(a.groups[i].center.x() - b.groups[i].center.x(),
                           a.groups[i].center.y() - b.groups[i].center.y()) < 1e-6);
        QVERIFY(qAbs(a.groups[i].radius - b.groups[i].radius) < 1e-6);
        QCOMPARE(a.groups[i].node, b.groups[i].node);
    }
    QCOMPARE(a.bubbles.size(), b.bubbles.size());
    for (int i = 0; i < a.bubbles.size(); ++i) {
        QVERIFY(std::hypot(a.bubbles[i].center.x() - b.bubbles[i].center.x(),
                           a.bubbles[i].center.y() - b.bubbles[i].center.y()) < 1e-6);
        QVERIFY(qAbs(a.bubbles[i].radius - b.bubbles[i].radius) < 1e-6);
        QCOMPARE(a.bubbles[i].node, b.bubbles[i].node);
    }
}

// A ten-group-by-sixty-child tree big enough that every group actually packs
// (i.e. clears minGroupR) at 1180x600 — used both for the cache-effectiveness
// tests below and (throwaway, not asserted on time) for the cold/warm
// build() timing measurement quoted in the SSO-24963 fix-wave report.
DirSizeNodePtr tenGroupsSixtyChildrenTree()
{
    auto root = std::make_shared<DirSizeNode>();
    root->name = "root"; root->path = "/root"; root->isDir = true;
    qint64 total = 0;
    for (int g = 0; g < 10; ++g) {
        DirSizeNode *grp = add(root.get(), mk(QString("g%1").arg(g), 0, true));
        qint64 gsize = 0;
        for (int c = 0; c < 60; ++c) {
            const qint64 size = std::max<qint64>(1, qint64(2'000'000.0 / std::pow(c + 1, 1.2)));
            add(grp, mk(QString("f%1").arg(c), size, false));
            gsize += size;
        }
        grp->size = gsize;
        total += gsize;
    }
    root->size = total;
    return root;
}
}

// SSO-24963 review round 2: build() with a warm PackCache must produce the
// exact same geometry as build() with no cache at all, for two different
// area sizes reusing the same cache.
void TestBubbleLayout::packCache_matchesUncachedGeometry()
{
    auto root = mk("root", 1000, true);
    DirSizeNode *big = add(root.get(), mk("big", 900, true));
    add(big, mk("a", 600, false));
    add(big, mk("b", 300, false));
    add(root.get(), mk("file", 100, false));

    BubbleLayout::PackCache cache;
    const QRectF area1(0, 0, 800, 600);
    const QRectF area2(0, 0, 1200, 500);

    const auto cold1 = BubbleLayout::build(root.get(), area1);
    const auto warm1 = BubbleLayout::build(root.get(), area1, BubbleLayout::Metrics(), &cache);
    compareResults(cold1, warm1);

    const auto cold2 = BubbleLayout::build(root.get(), area2);
    const auto warm2 = BubbleLayout::build(root.get(), area2, BubbleLayout::Metrics(), &cache);
    compareResults(cold2, warm2);
}

// SSO-24963 review round 2 (Critical 1): a second build() against a warm
// cache, same tree and area, must perform zero new packing work.
void TestBubbleLayout::packCache_warmSecondBuild_noNewPacks()
{
    DirSizeNodePtr root = tenGroupsSixtyChildrenTree();

    BubbleLayout::PackCache cache;
    const QRectF area(0, 0, 1180, 600);
    const auto first = BubbleLayout::build(root.get(), area, BubbleLayout::Metrics(), &cache);
    QVERIFY2(!first.groups.isEmpty(), "test tree should produce at least one group");
    const int afterFirst = cache.packCount();
    QVERIFY(afterFirst > 0);

    const auto second = BubbleLayout::build(root.get(), area, BubbleLayout::Metrics(), &cache);
    QCOMPARE(cache.packCount(), afterFirst);
    compareResults(first, second);
}

void TestBubbleLayout::packCache_clear_forcesRepack()
{
    auto root = mk("root", 1000, true);
    DirSizeNode *big = add(root.get(), mk("big", 900, true));
    add(big, mk("a", 600, false));
    add(big, mk("b", 300, false));
    add(root.get(), mk("file", 100, false));

    BubbleLayout::PackCache cache;
    const QRectF area(0, 0, 800, 600);
    BubbleLayout::build(root.get(), area, BubbleLayout::Metrics(), &cache);
    const int afterFirst = cache.packCount();
    QVERIFY(afterFirst > 0);

    BubbleLayout::build(root.get(), area, BubbleLayout::Metrics(), &cache);
    QCOMPARE(cache.packCount(), afterFirst);

    cache.clear();
    BubbleLayout::build(root.get(), area, BubbleLayout::Metrics(), &cache);
    QVERIFY(cache.packCount() > afterFirst);
}

// Moving the cold pack off the UI thread: packsCached() is what
// BubbleMapView asks to decide sync-vs-async, so it needs its own direct
// coverage beyond what the async view-level tests exercise indirectly.
void TestBubbleLayout::packsCached_trueForTrivialOrNullCache()
{
    // Null focus / empty tree: nothing to pack, trivially "cached" (no
    // async needed) regardless of whether a cache was even supplied.
    QVERIFY(BubbleLayout::packsCached(nullptr, QRectF(0, 0, 800, 600)));

    auto empty = mk("root", 0, true);
    QVERIFY(BubbleLayout::packsCached(empty.get(), QRectF(0, 0, 800, 600)));

    // A non-trivial tree but no cache to check against: never a "free"
    // build, since there's nothing that could have been cached.
    auto root = mk("root", 1000, true);
    add(root.get(), mk("file", 1000, false));
    QVERIFY(!BubbleLayout::packsCached(root.get(), QRectF(0, 0, 800, 600), BubbleLayout::Metrics(), nullptr));
}

void TestBubbleLayout::packsCached_falseOnMissTrueOnceWarm()
{
    DirSizeNodePtr root = tenGroupsSixtyChildrenTree();
    BubbleLayout::PackCache cache;
    const QRectF area(0, 0, 1180, 600);

    QVERIFY2(!BubbleLayout::packsCached(root.get(), area, BubbleLayout::Metrics(), &cache),
             "a fresh cache must report a miss for a tree that needs packing");

    BubbleLayout::build(root.get(), area, BubbleLayout::Metrics(), &cache);
    QVERIFY2(BubbleLayout::packsCached(root.get(), area, BubbleLayout::Metrics(), &cache),
             "after a real build, the identical request must report a hit");

    // A sufficiently different aspect ratio needs its own top-level pack
    // (see PackCache::aspectBucket()) even though the tree is unchanged.
    const QRectF wideArea(0, 0, 2000, 400);
    QVERIFY2(!BubbleLayout::packsCached(root.get(), wideArea, BubbleLayout::Metrics(), &cache),
             "a different aspect bucket is a fresh top-level-pack miss");
}

// The check must walk into group membership, not stop at the top-level
// pack: a cached top pack alone isn't enough if a group it produces still
// needs its own (uncached) nested pack.
void TestBubbleLayout::packsCached_falseWhenOnlyNestedPackIsMissing()
{
    auto root = mk("root", 1000, true);
    add(root.get(), mk("big", 900, true));
    DirSizeNode *big = root->children[0].get();
    add(big, mk("a", 600, false));
    add(big, mk("b", 300, false));
    add(root.get(), mk("file", 100, false));

    BubbleLayout::PackCache cache;
    const QRectF area(0, 0, 800, 600);

    // Warm only the top-level pack: an enormous minGroupR means nothing
    // ever qualifies as a group, so this build never calls nestedPack().
    BubbleLayout::Metrics noGroups;
    noGroups.minGroupR = 1e9;
    BubbleLayout::build(root.get(), area, noGroups, &cache);

    // Same tree/area/top-level key, but with the real metrics "big" now
    // wants a group whose nested pack was never computed.
    QVERIFY2(!BubbleLayout::packsCached(root.get(), area, BubbleLayout::Metrics(), &cache),
             "a cached top pack alone must not read as a hit when a group's own nested pack is missing");

    BubbleLayout::build(root.get(), area, BubbleLayout::Metrics(), &cache);
    QVERIFY2(BubbleLayout::packsCached(root.get(), area, BubbleLayout::Metrics(), &cache),
             "after the real build, both the top and nested packs are cached");
}

QTEST_APPLESS_MAIN(TestBubbleLayout)
#include "test_bubble_layout.moc"
