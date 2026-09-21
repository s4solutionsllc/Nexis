// SSO-24963: pins the arming invariants for the shared DiskMapView drill
// cross-fade (DiskMapView::armCrossFade()/startCrossFadeIfArmed()), exercised
// through BubbleMapView which opts into it — a hidden view must never fade,
// a shown view must fade on a genuine drill, and setRoot()/resize() mid-fade
// must cancel it (mirrors TestTreemapZoom's invariants for the geometric
// zoom transition).

#include <QtTest>
#include <QApplication>
#include <QColor>

#include <memory>

#include "Pages/DiskMap/bubble_map_view.h"
#include "Managers/dir_size_scanner.h"
#include "utilities.h"

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

// root(1000) -> big-dir(900, has children) + small-file(100).
DirSizeNodePtr buildTree()
{
    auto root = std::make_shared<DirSizeNode>();
    root->name = "root"; root->path = "/root"; root->isDir = true; root->size = 1000;
    DirSizeNode *big = add(root.get(), mk("big", 900, true));
    add(big, mk("a", 600, false));
    add(big, mk("b", 300, false));
    add(root.get(), mk("small", 100, false));
    return root;
}

void applyTestTheme(BubbleMapView &view)
{
    view.applyTheme(QColor("#f2f2f2"), QColor("#888888"), QColor("#202020"),
                     {QColor("#e95420"), QColor("#33cc99"), QColor("#4477ee")});
}

} // namespace

class TestDiskMapCrossFade : public QObject
{
    Q_OBJECT

private slots:
    void hiddenView_neverFadesOnDrillInto();
    void shownView_fadesOnDrillInto();
    void setRootMidFade_cancelsIt();
    void resizeMidFade_cancelsIt();
};

void TestDiskMapCrossFade::hiddenView_neverFadesOnDrillInto()
{
    BubbleMapView view;
    view.resize(800, 600);
    applyTestTheme(view);
    // Deliberately never shown.

    DirSizeNodePtr root = buildTree();
    view.setRoot(root);

    DirSizeNode *big = root->children[0].get();
    QVERIFY(big && big->isDir);

    view.drillInto(big);
    QVERIFY2(!view.isCrossFadeRunning(), "a hidden view must never cross-fade a drill");
    QCOMPARE(view.focus(), big);
}

void TestDiskMapCrossFade::shownView_fadesOnDrillInto()
{
    if (Utilities::prefersReducedMotion())
        QSKIP("System has reduce-motion enabled; the cross-fade is intentionally skipped there.");

    BubbleMapView view;
    view.resize(800, 600);
    applyTestTheme(view);
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));

    DirSizeNodePtr root = buildTree();
    view.setRoot(root);

    DirSizeNode *big = root->children[0].get();
    QVERIFY(big && big->isDir);

    view.drillInto(big);
    QVERIFY2(view.isCrossFadeRunning(),
             "drilling on a shown, non-reduced-motion view should start the cross-fade");
    QCOMPARE(view.focus(), big);
}

void TestDiskMapCrossFade::setRootMidFade_cancelsIt()
{
    if (Utilities::prefersReducedMotion())
        QSKIP("System has reduce-motion enabled; the cross-fade is intentionally skipped there.");

    BubbleMapView view;
    view.resize(800, 600);
    applyTestTheme(view);
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));

    DirSizeNodePtr root = buildTree();
    view.setRoot(root);

    DirSizeNode *big = root->children[0].get();
    QVERIFY(big && big->isDir);

    view.drillInto(big);
    QVERIFY2(view.isCrossFadeRunning(), "drilling should start the cross-fade");

    DirSizeNodePtr freshRoot = buildTree();
    view.setRoot(freshRoot);

    QVERIFY2(!view.isCrossFadeRunning(), "setRoot mid-fade must cancel it");
}

void TestDiskMapCrossFade::resizeMidFade_cancelsIt()
{
    if (Utilities::prefersReducedMotion())
        QSKIP("System has reduce-motion enabled; the cross-fade is intentionally skipped there.");

    BubbleMapView view;
    view.resize(800, 600);
    applyTestTheme(view);
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));

    DirSizeNodePtr root = buildTree();
    view.setRoot(root);

    DirSizeNode *big = root->children[0].get();
    QVERIFY(big && big->isDir);

    view.drillInto(big);
    QVERIFY2(view.isCrossFadeRunning(), "drilling should start the cross-fade");

    view.resize(700, 500);
    QVERIFY2(!view.isCrossFadeRunning(), "resizing mid-fade must cancel it");
}

QTEST_MAIN(TestDiskMapCrossFade)
#include "test_disk_map_crossfade.moc"
