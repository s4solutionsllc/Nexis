// Moving BubbleMapView's cold circle-packing off the UI thread: pins the
// synchronous fast path (warm cache never touches the worker), the async
// path (cold cache defers to a worker and lands byte-identical geometry to
// a synchronous BubbleLayout::build() with the same inputs), and the safety
// invariants a background worker racing the GUI thread needs — a
// superseded request's stale result is never applied (but the still-useful
// part of it is kept), a setRoot() mid-pack neither crashes nor corrupts
// the cache, destroying the view mid-pack doesn't crash, mouse input is
// ignored while a pack is pending, a burst of requests never fans out more
// than one worker at a time, and a stale (still-displayed) node's colour
// survives a newer focus's hue reassignment. Mirrors TestDiskMapCrossFade's
// fixture style.

#include <QtTest>
#include <QApplication>
#include <QCoreApplication>
#include <QColor>
#include <QSignalSpy>

#include <algorithm>
#include <cmath>
#include <memory>

#include "Pages/DiskMap/bubble_map_view.h"
#include "Managers/dir_size_scanner.h"
#include "dpi.h"

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

// The exact Metrics BubbleMapView::scaledMetrics() builds — a single source
// so a test comparing against a directly-called BubbleLayout::build() can't
// silently drift from the view's own metrics. (BubbleLayout::Metrics()'s
// own defaults happen to equal this today only because Dpi::factor() is 1.0
// in the test environment — this doesn't rely on that.)
BubbleLayout::Metrics viewMetrics()
{
    BubbleLayout::Metrics m;
    m.minGroupR   = Dpi::scale(45);
    m.innerPad    = Dpi::scale(4);
    m.labelBand   = Dpi::scale(16);
    m.minBubbleR  = 1.5;
    m.outerMargin = Dpi::scale(4);
    return m;
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

// Two comparably-large, differently-ranked top-level directories — used by
// the hue-stability test, which needs a node whose *correct* hue index is
// non-zero so it can tell "kept its real colour" apart from "silently fell
// back to the same default colour that happens to be correct".
DirSizeNodePtr buildTwoGroupTree()
{
    auto root = std::make_shared<DirSizeNode>();
    root->name = "root"; root->path = "/root"; root->isDir = true;

    DirSizeNode *groupA = add(root.get(), mk("groupA", 0, true));
    qint64 aSize = 0;
    for (int c = 0; c < 20; ++c) {
        const qint64 sz = 500 + c * 3;
        add(groupA, mk(QString("a%1").arg(c), sz, false));
        aSize += sz;
    }
    groupA->size = aSize;

    DirSizeNode *groupB = add(root.get(), mk("groupB", 0, true));
    qint64 bSize = 0;
    for (int c = 0; c < 20; ++c) {
        const qint64 sz = 400 + c * 3;
        add(groupB, mk(QString("b%1").arg(c), sz, false));
        bSize += sz;
    }
    groupB->size = bSize;

    root->size = aSize + bSize;
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
    void supersededRequest_dropsStaleResultButKeepsUsefulWork();
    void setRootMidPack_noCrashAndCorrectFinalLayout();
    void destroyingViewMidPack_doesNotCrash();
    void inputIgnoredWhilePackPending();
    void resizeBurst_dispatchesAtMostOneWorkerAtATime();
    void staleLayoutKeepsOriginalHueDuringPendingRepaint();
};

void TestDiskMapBubbleAsync::warmCache_resizeStaysSynchronous()
{
    BubbleMapView view;
    view.resize(800, 600);
    applyTestTheme(view);
    // A resize on a never-shown top-level widget doesn't reliably deliver
    // resizeEvent() (no native window yet) — show it, like every other
    // resize-driven test in this suite/TestDiskMapCrossFade.
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));

    DirSizeNodePtr root = buildBigTree();
    view.setRoot(root);
    QVERIFY(QTest::qWaitFor([&] { return !view.isPackPending(); }, 5000));

    const int countAfterFirstBuild = view.packCount();
    QVERIFY2(countAfterFirstBuild > 0, "the first build against an empty cache must have computed something");
    QVERIFY(!view.layout().groups.isEmpty() || !view.layout().bubbles.isEmpty());
    const BubbleLayout::Result before = view.layout();

    // A small resize that keeps the same quantised aspect bucket (see
    // PackCache::aspectBucket()) so the top-level pack stays a hit too —
    // every pack this rebuild needs is already cached.
    view.resize(796, 596);
    QCoreApplication::processEvents();

    QVERIFY2(!view.isPackPending(), "a resize with a fully warm cache must never go async");
    QCOMPARE(view.packCount(), countAfterFirstBuild);

    // Prove the resize was actually delivered and re-laid-out (not just
    // that nothing crashed) — the first group's geometry must reflect the
    // new, smaller area, not the pre-resize one.
    QVERIFY2(!before.groups.isEmpty(), "test tree should produce at least one group");
    const auto &groupBefore = before.groups[0];
    const auto groupAfter = std::find_if(view.layout().groups.begin(), view.layout().groups.end(),
        [&](const BubbleLayout::Group &g) { return g.node == groupBefore.node; });
    QVERIFY(groupAfter != view.layout().groups.end());
    const bool moved = std::abs(groupAfter->center.x() - groupBefore.center.x()) > 1e-6
                     || std::abs(groupAfter->center.y() - groupBefore.center.y()) > 1e-6
                     || std::abs(groupAfter->radius - groupBefore.radius) > 1e-6;
    QVERIFY2(moved, "resize must actually change the applied geometry, not just leave it untouched");
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
        BubbleLayout::build(root.get(), QRectF(view.rect()), viewMetrics(), &refCache);
    compareLayouts(view.layout(), expected);
}

// SSO review round 1 (Important 3): the original version of this test
// asserted only that the final focus/geometry matched group0 — true
// whether or not the stale setRoot() result was ever applied, since the
// drill's own (later) pack always overwrites mLayout regardless. Assert
// something only the coalesce-then-redispatch mechanism actually produces:
// the drill coalesces onto the still-running setRoot() worker (same tree,
// but a different key — see dispatchPackIfNeeded()) rather than firing a
// second one in parallel, so the total dispatch count is exactly two — one
// for root's own request, and one more once onPackFinished() re-checks and
// finds group0's own pack still missing.
void TestDiskMapBubbleAsync::supersededRequest_dropsStaleResultButKeepsUsefulWork()
{
    BubbleMapView view;
    view.resize(800, 600);
    applyTestTheme(view);

    DirSizeNodePtr root = buildBigTree();
    view.setRoot(root);
    QVERIFY(view.isPackPending());
    QCOMPARE(view.packDispatchCount(), 1);

    DirSizeNode *group0 = root->children[0].get();
    QVERIFY(group0 && group0->isDir);
    view.drillInto(group0); // supersedes the still-in-flight setRoot() pack

    QVERIFY(QTest::qWaitFor([&] { return !view.isPackPending(); }, 5000));
    QCOMPARE(view.focus(), group0);
    QCOMPARE(view.packDispatchCount(), 2);

    BubbleLayout::PackCache refCache;
    const BubbleLayout::Result expected =
        BubbleLayout::build(group0, QRectF(view.rect()), viewMetrics(), &refCache);
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
        BubbleLayout::build(root2.get(), QRectF(view.rect()), viewMetrics(), &refCache);
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

// SSO review round 1 (Important 1): a burst of rebuild-triggering requests
// that all land on the SAME (focus, aspect-bucket) PackKey — e.g. a resize
// drag settling near its final size, or jitter around it, or the lockstep
// hidden view re-asking for what the visible one already triggered — must
// coalesce onto whichever single worker is already in flight for that key
// (dispatchPackIfNeeded()), not fire one redundant ~hundreds-of-ms job per
// request. (Distinct-key bursts, e.g. a resize drag walking many different
// aspect buckets, each still dispatch their own — same-key dedup can't
// help there — but bubblePackThreadPool()'s maxThreadCount(1) still keeps
// them from ever executing *concurrently* or competing with
// DirSizeScanner's own worker; that part isn't itself observable from the
// dispatch count, so it isn't asserted here.) Assert on the dispatch count
// directly rather than on timing.
void TestDiskMapBubbleAsync::resizeBurst_dispatchesAtMostOneWorkerAtATime()
{
    BubbleMapView view;
    // Portrait — a deliberately different aspect bucket (see
    // PackCache::aspectBucket()) from the burst's landscape 4:3 sizes
    // below, so the initial setRoot() dispatch and the burst's coalesced
    // dispatch are provably two distinct keys, not an accidental hit on the
    // same one.
    view.resize(300, 400);
    applyTestTheme(view);
    // Shown (and exposed) *before* setRoot()/the resize burst below, not
    // in between — resize() on a never-shown widget doesn't reliably
    // deliver resizeEvent(), but waiting for exposure here, before the
    // burst starts, still leaves the burst itself running with no event
    // loop turn in between (what it's actually testing).
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));

    DirSizeNodePtr root = buildBigTree();
    view.setRoot(root);
    QVERIFY(view.isPackPending());
    const int afterSetRoot = view.packDispatchCount();
    QCOMPARE(afterSetRoot, 1);

    // A burst of resizes to several DISTINCT sizes (each a genuine
    // resizeEvent — Qt no-ops a resize() to an unchanged size) that all
    // keep exactly the same 4:3 aspect ratio, so every one of them quantises
    // to the same PackKey (see PackCache::aspectBucket()) as the first —
    // fired with no event loop turn in between, so the very first one is
    // still the only thing ever dispatched.
    const QList<QSize> sameKeySizes = {
        QSize(800, 600), QSize(804, 603), QSize(796, 597), QSize(808, 606),
        QSize(792, 594), QSize(812, 609), QSize(788, 591), QSize(816, 612),
    };
    for (const QSize &s : sameKeySizes) {
        view.resize(s);
        QVERIFY(view.isPackPending());
    }

    // Exactly one MORE dispatch than the original setRoot() (whose own key,
    // for the initial 400x300 size, differs from this burst's) — every
    // repeat of the burst's own shared key must have coalesced onto that
    // one dispatch, not fired a worker per resize.
    QCOMPARE(view.packDispatchCount(), afterSetRoot + 1);

    QVERIFY(QTest::qWaitFor([&] { return !view.isPackPending(); }, 5000));

    // The result must still be structurally sound for the final size —
    // not a byte-identical match to a *fresh* build() at that exact size,
    // since the coalesced worker packed against the aspect *ratio* it was
    // dispatched with (800x600's), not the burst's later, same-bucket-but-
    // not-identical ones (see PackCache::aspectBucket() — that's the
    // approximation the whole bucket cache deliberately accepts across a
    // resize). Every bubble/group must still land inside the current
    // viewport, which a stale-aspect-but-live-rect fit() still guarantees.
    QVERIFY2(!view.layout().groups.isEmpty() || !view.layout().bubbles.isEmpty(),
             "the coalesced pack must still have produced real geometry");
    const QRectF bounds(view.rect());
    for (const auto &g : view.layout().groups)
        QVERIFY2(bounds.contains(QRectF(g.center.x() - g.radius, g.center.y() - g.radius,
                                        g.radius * 2, g.radius * 2).adjusted(1, 1, -1, -1)),
                 "group extends outside the current viewport");
    for (const auto &b : view.layout().bubbles)
        QVERIFY2(bounds.contains(QRectF(b.center.x() - b.radius, b.center.y() - b.radius,
                                        b.radius * 2, b.radius * 2).adjusted(1, 1, -1, -1)),
                 "bubble extends outside the current viewport");
}

// SSO review round 1 (Important 2): setRoot()/drillInto()/drillUp() all
// refresh DiskMapView's live hue map for the NEW focus synchronously,
// before rebuildLayout() even runs. Since BubbleMapView deliberately keeps
// painting an OLDER focus's nodes while a newer request's pack is still in
// flight, resolving their colours against that live map (which no longer
// has entries for them) used to fall back to the same default colour for
// everything. BubbleMapView must instead resolve them against the hue
// snapshot frozen alongside mDisplayedTree/mLayout.
void TestDiskMapBubbleAsync::staleLayoutKeepsOriginalHueDuringPendingRepaint()
{
    BubbleMapView view;
    view.resize(800, 600);
    applyTestTheme(view);

    DirSizeNodePtr root = buildTwoGroupTree();
    view.setRoot(root);
    QVERIFY(QTest::qWaitFor([&] { return !view.isPackPending(); }, 5000));
    QVERIFY2(view.layout().groups.size() >= 2, "both directories should be large enough to frame as groups");

    DirSizeNode *groupB = root->children[1].get();
    QCOMPARE(QString(groupB->name), QString("groupB"));

    const QColor before = view.debugColourForDisplayedNode(groupB);
    // groupA is larger, so it gets hue index 0 (mPalette[0], "#e95420")
    // and groupB gets hue index 1 — sanity-check the fixture actually
    // exercises a non-default hue before relying on that below.
    QVERIFY2(before != QColor("#e95420"), "groupB must not already be at the default/hue-0 colour");

    // Drilling into groupB needs its own top-level pack (never computed
    // while root was in focus, only its NESTED pack was), so this is
    // guaranteed async — and assignHues() has already reassigned the live
    // hue map for groupB's own children by the time this returns, so
    // "groupB" itself (now the focus, not a child of anything) is no
    // longer a key in it at all.
    view.drillInto(groupB);
    QVERIFY(view.isPackPending());

    const QColor stale = view.debugColourForDisplayedNode(groupB);
    QCOMPARE(stale, before);
    QVERIFY2(stale != QColor("#e95420"),
             "a stale node must keep its own frozen colour, not fall back to the default hue-0 one");

    QVERIFY(QTest::qWaitFor([&] { return !view.isPackPending(); }, 5000));
}

QTEST_MAIN(TestDiskMapBubbleAsync)
#include "test_disk_map_bubble_async.moc"
