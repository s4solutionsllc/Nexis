// SSO-24963: pure-geometry tests for SunburstLayout — the two-ring sunburst
// layout consumed by SunburstView. Mirrors test_bubble_layout.cpp /
// test_treemap_layout.cpp's structure and helpers.

#include <QtTest>
#include "Pages/DiskMap/sunburst_layout.h"

#include <cmath>

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

class TestSunburstLayout : public QObject
{
    Q_OBJECT
private slots:
    void emptyOrNullFocus_yieldsNothing();
    void ring0SweepsSumTo360_proportionalToSize();
    void ring1WedgesLieWithinParentSpan_andSumToParentSweep();
    void fileOrEmptyDirRing0_producesRing1PlaceholderSameNode();
    void radiiOrdering_andDiscFitsArea();
    void hitTest_ring1ChildRing0ParentHubOutside_andSeamBoundary();
};

void TestSunburstLayout::emptyOrNullFocus_yieldsNothing()
{
    auto r = SunburstLayout::build(nullptr, QRectF(0, 0, 800, 600));
    QVERIFY(r.wedges.isEmpty());

    auto root = mk("root", 0, true);
    r = SunburstLayout::build(root.get(), QRectF(0, 0, 800, 600));
    QVERIFY(r.wedges.isEmpty());

    // Directory with only zero-size children also yields nothing.
    auto root2 = mk("root2", 0, true);
    add(root2.get(), mk("empty", 0, true));
    r = SunburstLayout::build(root2.get(), QRectF(0, 0, 800, 600));
    QVERIFY(r.wedges.isEmpty());
}

void TestSunburstLayout::ring0SweepsSumTo360_proportionalToSize()
{
    auto root = mk("root", 600, true);
    DirSizeNode *a = add(root.get(), mk("a", 300, false));
    DirSizeNode *b = add(root.get(), mk("b", 200, false));
    DirSizeNode *c = add(root.get(), mk("c", 100, false));

    const auto r = SunburstLayout::build(root.get(), QRectF(0, 0, 800, 600));

    qreal sum = 0;
    qreal sweepA = 0, sweepB = 0, sweepC = 0;
    for (const auto &w : r.wedges) {
        if (w.ring != 0)
            continue;
        sum += w.sweepDeg;
        if (w.node == a) sweepA = w.sweepDeg;
        if (w.node == b) sweepB = w.sweepDeg;
        if (w.node == c) sweepC = w.sweepDeg;
    }
    QVERIFY2(qAbs(sum - 360.0) < 0.01, qPrintable(QString("sum=%1").arg(sum)));
    QVERIFY2(qAbs(sweepA / sweepB - 1.5) < 0.01, "a:b should be 300:200 = 1.5");
    QVERIFY2(qAbs(sweepB / sweepC - 2.0) < 0.01, "b:c should be 200:100 = 2.0");
}

void TestSunburstLayout::ring1WedgesLieWithinParentSpan_andSumToParentSweep()
{
    auto root = mk("root", 1000, true);
    DirSizeNode *big = add(root.get(), mk("big", 1000, true));
    add(big, mk("x", 700, false));
    add(big, mk("y", 300, false));

    const auto r = SunburstLayout::build(root.get(), QRectF(0, 0, 800, 600));

    const SunburstLayout::Wedge *parent = nullptr;
    for (const auto &w : r.wedges)
        if (w.ring == 0 && w.node == big) parent = &w;
    QVERIFY(parent);

    qreal childSum = 0;
    for (const auto &w : r.wedges) {
        if (w.ring != 1 || w.parentIndex < 0 || r.wedges[w.parentIndex].node != big)
            continue;
        QVERIFY2(w.startDeg >= parent->startDeg - 0.01, "ring-1 wedge starts before parent span");
        QVERIFY2(w.startDeg + w.sweepDeg <= parent->startDeg + parent->sweepDeg + 0.01,
                 "ring-1 wedge ends after parent span");
        childSum += w.sweepDeg;
    }
    QVERIFY2(qAbs(childSum - parent->sweepDeg) < 0.01,
             qPrintable(QString("childSum=%1 parentSweep=%2").arg(childSum).arg(parent->sweepDeg)));
}

void TestSunburstLayout::fileOrEmptyDirRing0_producesRing1PlaceholderSameNode()
{
    auto root = mk("root", 1000, true);
    DirSizeNode *file = add(root.get(), mk("readme.txt", 500, false));
    DirSizeNode *emptyDir = add(root.get(), mk("cache", 500, true));
    // emptyDir has no size>0 children.

    const auto r = SunburstLayout::build(root.get(), QRectF(0, 0, 800, 600));

    auto findPlaceholder = [&](DirSizeNode *node) -> const SunburstLayout::Wedge* {
        for (const auto &w : r.wedges)
            if (w.ring == 1 && w.node == node) return &w;
        return nullptr;
    };

    const SunburstLayout::Wedge *fileRing0 = nullptr, *dirRing0 = nullptr;
    for (const auto &w : r.wedges) {
        if (w.ring == 0 && w.node == file) fileRing0 = &w;
        if (w.ring == 0 && w.node == emptyDir) dirRing0 = &w;
    }
    QVERIFY(fileRing0 && dirRing0);

    const auto *filePlaceholder = findPlaceholder(file);
    const auto *dirPlaceholder = findPlaceholder(emptyDir);
    QVERIFY(filePlaceholder && filePlaceholder->placeholder);
    QVERIFY(dirPlaceholder && dirPlaceholder->placeholder);

    QCOMPARE(filePlaceholder->startDeg, fileRing0->startDeg);
    QVERIFY(qAbs(filePlaceholder->sweepDeg - fileRing0->sweepDeg) < 1e-9);
    QCOMPARE(dirPlaceholder->startDeg, dirRing0->startDeg);
    QVERIFY(qAbs(dirPlaceholder->sweepDeg - dirRing0->sweepDeg) < 1e-9);
}

void TestSunburstLayout::radiiOrdering_andDiscFitsArea()
{
    auto root = mk("root", 1000, true);
    DirSizeNode *big = add(root.get(), mk("big", 700, true));
    add(big, mk("x", 700, false));
    add(root.get(), mk("small", 300, false));

    const QRectF area(0, 0, 900, 700);
    const auto r = SunburstLayout::build(root.get(), area);

    QVERIFY(r.hubR < r.ring0InnerR);
    QVERIFY(r.ring0InnerR <= r.ring0OuterR);
    QVERIFY(r.ring0OuterR < r.ring1InnerR);
    QVERIFY(r.ring1InnerR <= r.outerR);
    QCOMPARE(r.ring1OuterR, r.outerR);

    const QRectF disc(r.center.x() - r.outerR, r.center.y() - r.outerR, r.outerR * 2, r.outerR * 2);
    QVERIFY2(area.contains(disc.adjusted(1, 1, -1, -1)), "disc must fit inside area minus margin");
}

void TestSunburstLayout::hitTest_ring1ChildRing0ParentHubOutside_andSeamBoundary()
{
    auto root = mk("root", 1000, true);
    DirSizeNode *big = add(root.get(), mk("big", 1000, true));
    DirSizeNode *leaf = add(big, mk("leaf", 1000, false));

    const auto r = SunburstLayout::build(root.get(), QRectF(0, 0, 800, 600));
    QVERIFY(!r.wedges.isEmpty());

    const SunburstLayout::Wedge *ring0 = nullptr, *ring1 = nullptr;
    for (const auto &w : r.wedges) {
        if (w.ring == 0) ring0 = &w;
        if (w.ring == 1) ring1 = &w;
    }
    QVERIFY(ring0 && ring1);
    QCOMPARE(ring0->node, big);
    QCOMPARE(ring1->node, leaf);

    // Single child spans the whole 360°: mid-angle 0° (straight up) at
    // mid-radius of each ring.
    const qreal ring0MidR = (r.ring0InnerR + r.ring0OuterR) / 2.0;
    const qreal ring1MidR = (r.ring1InnerR + r.ring1OuterR) / 2.0;
    const QPointF ring0Point = r.center + QPointF(0, -ring0MidR);
    const QPointF ring1Point = r.center + QPointF(0, -ring1MidR);

    QCOMPARE(SunburstLayout::hitTest(r, ring0Point), big);
    QCOMPARE(SunburstLayout::hitTest(r, ring1Point), leaf);

    // Hub and far outside both miss.
    QCOMPARE(SunburstLayout::hitTest(r, r.center), static_cast<DirSizeNode*>(nullptr));
    QCOMPARE(SunburstLayout::hitTest(r, r.center + QPointF(r.outerR * 5, 0)),
             static_cast<DirSizeNode*>(nullptr));

    // Gap between ring 0 and ring 1 (the ringGap band) misses too.
    const qreal gapR = (r.ring0OuterR + r.ring1InnerR) / 2.0;
    QCOMPARE(SunburstLayout::hitTest(r, r.center + QPointF(0, -gapR)),
             static_cast<DirSizeNode*>(nullptr));

    // Seam robustness: a point a hair before the 0°/360° boundary (deg ~
    // 359.999) still resolves to the single all-encompassing wedge rather
    // than falling through to nullptr.
    const qreal nearSeamRad = 359.999 * M_PI / 180.0;
    const QPointF nearSeam = r.center + QPointF(ring1MidR * std::sin(nearSeamRad),
                                                -ring1MidR * std::cos(nearSeamRad));
    QCOMPARE(SunburstLayout::hitTest(r, nearSeam), leaf);

    // A wedge that genuinely straddles the seam (start=350°, sweep=20°,
    // ending at the equivalent of 10°) must hit for angles on both sides of
    // 0°/360°, and miss on the opposite side of the circle.
    SunburstLayout::Result synth;
    synth.center = QPointF(0, 0);
    synth.hubR = 10;
    synth.outerR = 100;
    SunburstLayout::Wedge seam;
    seam.startDeg = 350;
    seam.sweepDeg = 20;
    seam.innerR = 20;
    seam.outerR = 90;
    seam.node = leaf;
    synth.wedges.append(seam);

    auto pointAt = [](qreal deg, qreal radius) {
        const qreal rad = deg * M_PI / 180.0;
        return QPointF(radius * std::sin(rad), -radius * std::cos(rad));
    };
    QCOMPARE(SunburstLayout::hitTest(synth, pointAt(355.0, 50)), leaf);
    QCOMPARE(SunburstLayout::hitTest(synth, pointAt(5.0, 50)), leaf);
    QCOMPARE(SunburstLayout::hitTest(synth, pointAt(180.0, 50)),
             static_cast<DirSizeNode*>(nullptr));
}

QTEST_APPLESS_MAIN(TestSunburstLayout)
#include "test_sunburst_layout.moc"
