// Moving BubbleMapView's cold circle-packing off the UI thread: pins the
// synchronous fast path (warm cache never touches the worker), the async
// path (cold cache defers to a worker and lands byte-identical geometry to
// a synchronous BubbleLayout::build() with the same inputs), and the safety
// invariants a background worker racing the GUI thread needs — a
// superseded request's result is dropped, a setRoot() mid-pack neither
// crashes nor corrupts the cache, destroying the view mid-pack doesn't
// crash, and mouse input is ignored while a pack is pending. Mirrors
// TestDiskMapCrossFade's fixture style.

#include <QtTest>
#include <QApplication>
#include <QCoreApplication>
#include <QColor>
#include <QSignalSpy>

#include <cmath>
#include <memory>

#include "Pages/DiskMap/bubble_map_view.h"
#include "Managers/dir_size_scanner.h"

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

// 10 groups x 60 children each — deliberately mirrors the brief's own
// "measured ~400ms" cold-cache shape, so on a fresh PackCache this is never
// a pure cache hit (see BubbleLayout::packsCached()) and always takes the
// async path on the very first build.
DirSizeNodePtr buildBigTree()
{
    auto root = std::make_shared<DirSizeNode>();
    root->name = "root"; root->path = "/root"; root->isDir = true;
    qint64 total = 0;
    for (int g = 0; g < 10; ++g) {
        DirSizeNode *group = add(root.get(), mk(QString("group%1").arg(g), 0, true));
        qint64 groupSize = 0;
        for (int c = 0; c < 60; ++c) {
            const qint64 sz = 1000 + c * 7;
            add(group, mk(QString("child%1").arg(c), sz, false));
            groupSize += sz;
        }
        group->size = groupSize;
        total += groupSize;
    }
    root->size = total;
    return root;
}

void applyTestTheme(BubbleMapView &view)
{
    view.applyTheme(QColor("#f2f2f2"), QColor("#888888"), QColor("#202020"),
                     {QColor("#e95420"), QColor("#33cc99"), QColor("#4477ee")});
}

// Byte-identical (within 1e-6) geometry and matching node identity —
// BubbleMapView's applied mLayout must be indistinguishable from a plain
// synchronous BubbleLayout::build() call with the same focus/area/metrics.
void compareLayouts(const BubbleLayout::Result &actual, const BubbleLayout::Result &expected)
{
    QCOMPARE(actual.groups.size(), expected.groups.size());
    QCOMPARE(actual.bubbles.size(), expected.bubbles.size());
    for (int i = 0; i < expected.groups.size(); ++i) {
        QCOMPARE(actual.groups[i].node, expected.groups[i].node);
        QVERIFY(std::abs(actual.groups[i].center.x() - expected.groups[i].center.x()) < 1e-6);
        QVERIFY(std::abs(actual.groups[i].center.y() - expected.groups[i].center.y()) < 1e-6);
        QVERIFY(std::abs(actual.groups[i].radius - expected.groups[i].radius) < 1e-6);
    }
    for (int i = 0; i < expected.bubbles.size(); ++i) {
        QCOMPARE(actual.bubbles[i].node, expected.bubbles[i].node);
        QVERIFY(std::abs(actual.bubbles[i].center.x() - expected.bubbles[i].center.x()) < 1e-6);
        QVERIFY(std::abs(actual.bubbles[i].center.y() - expected.bubbles[i].center.y()) < 1e-6);
        QVERIFY(std::abs(actual.bubbles[i].radius - expected.bubbles[i].radius) < 1e-6);
    }
}

} // namespace

class TestDiskMapBubbleAsync : public QObject
{
    Q_OBJECT

private slots:
    void warmCache_resizeStaysSynchronous();
    void coldCache_setRootIsAsyncAndMatchesSyncBuild();
    void supersededRequest_dropsStaleResult();
    void setRootMidPack_noCrashAndCorrectFinalLayout();
    void destroyingViewMidPack_doesNotCrash();
    void inputIgnoredWhilePackPending();
};

void TestDiskMapBubbleAsync::warmCache_resizeStaysSynchronous()
{
    BubbleMapView view;
    view.resize(800, 600);
    applyTestTheme(view);

    DirSizeNodePtr root = buildBigTree();
    view.setRoot(root);
    QVERIFY(QTest::qWaitFor([&] { return !view.isPackPending(); }, 5000));

    const int countAfterFirstBuild = view.packCount();
    QVERIFY2(countAfterFirstBuild > 0, "the first build against an empty cache must have computed something");
    QVERIFY(!view.layout().groups.isEmpty() || !view.layout().bubbles.isEmpty());

    // A small resize that keeps the same quantised aspect bucket (see
    // PackCache::aspectBucket()) so the top-level pack stays a hit too —
    // every pack this rebuild needs is already cached.
    view.resize(796, 596);
    QCoreApplication::processEvents();

    QVERIFY2(!view.isPackPending(), "a resize with a fully warm cache must never go async");
    QCOMPARE(view.packCount(), countAfterFirstBuild);
}

void TestDiskMapBubbleAsync::coldCache_setRootIsAsyncAndMatchesSyncBuild()
{
    BubbleMapView view;
    view.resize(800, 600);
    applyTestTheme(view);

    DirSizeNodePtr root = buildBigTree();
    view.setRoot(root);
    QVERIFY2(view.isPackPending(), "setRoot() on a tree that needs packing must leave a pending state");

    QVERIFY(QTest::qWaitFor([&] { return !view.isPackPending(); }, 5000));

    BubbleLayout::PackCache refCache;
    const BubbleLayout::Result expected =
        BubbleLayout::build(root.get(), QRectF(view.rect()), BubbleLayout::Metrics(), &refCache);
    compareLayouts(view.layout(), expected);
}

void TestDiskMapBubbleAsync::supersededRequest_dropsStaleResult()
{
    BubbleMapView view;
    view.resize(800, 600);
    applyTestTheme(view);

    DirSizeNodePtr root = buildBigTree();
    view.setRoot(root);
    QVERIFY(view.isPackPending());

    DirSizeNode *group0 = root->children[0].get();
    QVERIFY(group0 && group0->isDir);
    view.drillInto(group0); // supersedes the still-in-flight setRoot() pack

    QVERIFY(QTest::qWaitFor([&] { return !view.isPackPending(); }, 5000));
    QCOMPARE(view.focus(), group0);

    BubbleLayout::PackCache refCache;
    const BubbleLayout::Result expected =
        BubbleLayout::build(group0, QRectF(view.rect()), BubbleLayout::Metrics(), &refCache);
    compareLayouts(view.layout(), expected);
}

void TestDiskMapBubbleAsync::setRootMidPack_noCrashAndCorrectFinalLayout()
{
    BubbleMapView view;
    view.resize(800, 600);
    applyTestTheme(view);

    DirSizeNodePtr root1 = buildBigTree();
    view.setRoot(root1);
    QVERIFY(view.isPackPending());

    // A second, independent tree — its worker's result must never be
    // merged against root1's (now-cleared, per rootAboutToChange()) cache.
    DirSizeNodePtr root2 = buildBigTree();
    view.setRoot(root2);
    QVERIFY(view.isPackPending());

    QVERIFY(QTest::qWaitFor([&] { return !view.isPackPending(); }, 5000));
    QCOMPARE(view.focus(), root2.get());

    BubbleLayout::PackCache refCache;
    const BubbleLayout::Result expected =
        BubbleLayout::build(root2.get(), QRectF(view.rect()), BubbleLayout::Metrics(), &refCache);
    compareLayouts(view.layout(), expected);
}

void TestDiskMapBubbleAsync::destroyingViewMidPack_doesNotCrash()
{
    DirSizeNodePtr root = buildBigTree();
    {
        BubbleMapView view;
        view.resize(800, 600);
        applyTestTheme(view);
        view.setRoot(root);
        QVERIFY(view.isPackPending());
    } // view destroyed here — the worker (parented QFutureWatcher) must not
      // touch it once it finishes.

    // Give the thread pool a chance to actually run the now-orphaned task
    // to completion; reaching this point at all (no crash/UB) is the
    // assertion.
    QTest::qWait(300);
    QVERIFY(true);
}

void TestDiskMapBubbleAsync::inputIgnoredWhilePackPending()
{
    BubbleMapView view;
    view.resize(800, 600);
    applyTestTheme(view);
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));

    view.setRoot(buildBigTree());
    QVERIFY(QTest::qWaitFor([&] { return !view.isPackPending(); }, 5000));
    QVERIFY(!view.layout().groups.isEmpty());

    // A point guaranteed to land on a real, currently-painted directory
    // group — the stale layout stays on screen (mLayout is never cleared)
    // while the next pack is pending, so this is a genuine hit if the
    // pending guard didn't exist.
    const QPointF hitPoint = view.layout().groups[0].center;

    view.setRoot(buildBigTree()); // a second, independent cold tree
    QVERIFY(view.isPackPending());

    const QPointF globalHitPoint = view.mapToGlobal(hitPoint.toPoint());

    QSignalSpy drillSpy(&view, &DiskMapView::drillRequested);
    QMouseEvent dbl(QEvent::MouseButtonDblClick, hitPoint, globalHitPoint, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(&view, &dbl);
    QCOMPARE(drillSpy.count(), 0);

    QSignalSpy hoverSpy(&view, &DiskMapView::tileHovered);
    QMouseEvent move(QEvent::MouseMove, hitPoint, globalHitPoint, Qt::NoButton, Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(&view, &move);
    QCOMPARE(hoverSpy.count(), 0);

    QVERIFY(QTest::qWaitFor([&] { return !view.isPackPending(); }, 5000));
}

QTEST_MAIN(TestDiskMapBubbleAsync)
#include "test_disk_map_bubble_async.moc"
