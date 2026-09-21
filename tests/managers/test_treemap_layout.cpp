#include <QtTest>
#include "Pages/DiskMap/treemap_layout.h"

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

class TestTreemapLayout : public QObject
{
    Q_OBJECT
private slots:
    void emptyOrNullFocus_yieldsNothing();
    void bigDirectory_becomesFrameWithChildrenInsideContent();
    void tinyDirectory_collapsesToLeafTile();
    void tiles_doNotOverlap();
    void topLevelAreas_areProportionalToSize();
    void hitTest_prefersTileThenFrameHeader();
};

void TestTreemapLayout::emptyOrNullFocus_yieldsNothing()
{
    auto r = TreemapLayout::build(nullptr, QRectF(0, 0, 800, 600));
    QVERIFY(r.frames.isEmpty() && r.tiles.isEmpty());
    auto root = mk("root", 0, true);
    r = TreemapLayout::build(root.get(), QRectF(0, 0, 800, 600));
    QVERIFY(r.frames.isEmpty() && r.tiles.isEmpty());
}

void TestTreemapLayout::bigDirectory_becomesFrameWithChildrenInsideContent()
{
    auto root = mk("root", 1000, true);
    DirSizeNode *big = add(root.get(), mk("big", 900, true));
    add(big, mk("a", 600, false));
    add(big, mk("b", 300, false));
    add(root.get(), mk("file", 100, false));

    const auto r = TreemapLayout::build(root.get(), QRectF(0, 0, 800, 600));
    QCOMPARE(r.frames.size(), 1);
    QCOMPARE(r.frames[0].node, big);
    QVERIFY(r.frames[0].outer.contains(r.frames[0].header));
    QVERIFY(r.frames[0].outer.contains(r.frames[0].content));
    int inside = 0;
    for (const auto &t : r.tiles) {
        if (t.frameIndex == 0) {
            QVERIFY(r.frames[0].content.contains(t.rect));
            ++inside;
        }
    }
    QCOMPARE(inside, 2);
}

void TestTreemapLayout::tinyDirectory_collapsesToLeafTile()
{
    auto root = mk("root", 10000, true);
    add(root.get(), mk("huge", 9990, false));
    DirSizeNode *tiny = add(root.get(), mk("tiny", 10, true));
    add(tiny, mk("x", 10, false));

    const auto r = TreemapLayout::build(root.get(), QRectF(0, 0, 800, 600));
    QVERIFY(r.frames.isEmpty());
    bool found = false;
    for (const auto &t : r.tiles)
        if (t.node == tiny) { found = true; QCOMPARE(t.frameIndex, -1); }
    QVERIFY(found);
}

void TestTreemapLayout::tiles_doNotOverlap()
{
    auto root = mk("root", 0, true);
    for (int i = 1; i <= 12; ++i) {
        DirSizeNode *d = add(root.get(), mk(QString("d%1").arg(i), 0, true));
        for (int j = 1; j <= 5; ++j) { add(d, mk(QString("f%1").arg(j), i * j * 10, false)); d->size += i * j * 10; }
        root->size += d->size;
    }
    const auto r = TreemapLayout::build(root.get(), QRectF(0, 0, 1000, 700));
    for (int i = 0; i < r.tiles.size(); ++i)
        for (int j = i + 1; j < r.tiles.size(); ++j) {
            const QRectF x = r.tiles[i].rect.intersected(r.tiles[j].rect);
            QVERIFY2(x.width() < 0.5 || x.height() < 0.5, "tiles overlap");
        }
}

void TestTreemapLayout::topLevelAreas_areProportionalToSize()
{
    auto root = mk("root", 300, true);
    DirSizeNode *a = add(root.get(), mk("a", 200, false));
    DirSizeNode *b = add(root.get(), mk("b", 100, false));
    TreemapLayout::Metrics m; m.frameGap = 0; m.tileGap = 0;
    const auto r = TreemapLayout::build(root.get(), QRectF(0, 0, 600, 300), m);
    qreal aa = 0, ab = 0;
    for (const auto &t : r.tiles) {
        if (t.node == a) aa = t.rect.width() * t.rect.height();
        if (t.node == b) ab = t.rect.width() * t.rect.height();
    }
    QVERIFY(qAbs(aa / ab - 2.0) < 0.05);
}

void TestTreemapLayout::hitTest_prefersTileThenFrameHeader()
{
    auto root = mk("root", 1000, true);
    DirSizeNode *big = add(root.get(), mk("big", 1000, true));
    DirSizeNode *leaf = add(big, mk("leaf", 1000, false));
    const auto r = TreemapLayout::build(root.get(), QRectF(0, 0, 800, 600));
    QCOMPARE(TreemapLayout::hitTest(r, r.tiles[0].rect.center()), leaf);
    QCOMPARE(TreemapLayout::hitTest(r, r.frames[0].header.center()), big);
    QCOMPARE(TreemapLayout::hitTest(r, QPointF(-5, -5)), nullptr);
}

QTEST_APPLESS_MAIN(TestTreemapLayout)
#include "test_treemap_layout.moc"
