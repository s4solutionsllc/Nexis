// SSO-24963: pins the arming invariants for TreemapView's drill zoom
// transition — a hidden view (lockstep drill from another view while this
// one isn't shown) must never animate, a shown view with a genuinely framed
// directory must animate and a mid-animation resize must stop it, and a
// drill into a directory that lays out to nothing must stay instant rather
// than starting a 200ms animation with nothing visible to show for it.

#include <QtTest>
#include <QApplication>
#include <QColor>

#include <memory>

#include "Pages/DiskMap/treemap_view.h"
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

// root(1000) -> big-dir(900, framed: has children and is large enough at
// 800x600) + small-file(100).
DirSizeNodePtr buildTreeWithFramedDir()
{
    auto root = std::make_shared<DirSizeNode>();
    root->name = "root"; root->path = "/root"; root->isDir = true; root->size = 1000;
    DirSizeNode *big = add(root.get(), mk("big", 900, true));
    add(big, mk("a", 600, false));
    add(big, mk("b", 300, false));
    add(root.get(), mk("small", 100, false));
    return root;
}

// root(500) -> empty-dir(500, no children of its own, so drilling into it
// lays out to nothing).
DirSizeNodePtr buildTreeWithEmptyDir()
{
    auto root = std::make_shared<DirSizeNode>();
    root->name = "root"; root->path = "/root"; root->isDir = true; root->size = 500;
    add(root.get(), mk("empty", 500, true));
    return root;
}

void applyTestTheme(TreemapView &view)
{
    view.applyTheme(QColor("#f2f2f2"), QColor("#888888"), QColor("#202020"),
                     {QColor("#e95420"), QColor("#33cc99"), QColor("#4477ee")});
}

} // namespace

class TestTreemapZoom : public QObject
{
    Q_OBJECT

private slots:
    void hiddenView_doesNotAnimateOnDrillInto();
    void shownView_animatesOnDrillInto_andResizeStopsIt();
    void emptyDirectory_doesNotAnimate();
};

void TestTreemapZoom::hiddenView_doesNotAnimateOnDrillInto()
{
    TreemapView view;
    view.resize(800, 600);
    applyTestTheme(view);
    // Deliberately never shown — mirrors a lockstep drill reaching this view
    // while the page has it hidden behind another visualization mode.

    DirSizeNodePtr root = buildTreeWithFramedDir();
    view.setRoot(root);

    DirSizeNode *big = root->children[0].get();
    QVERIFY(big && big->isDir);

    view.drillInto(big);
    QVERIFY2(!view.isZoomRunning(), "a hidden view must never animate a drill");
    QCOMPARE(view.focus(), big);
}

void TestTreemapZoom::shownView_animatesOnDrillInto_andResizeStopsIt()
{
    if (Utilities::prefersReducedMotion())
        QSKIP("System has reduce-motion enabled; drill zoom is intentionally instant there.");

    TreemapView view;
    view.resize(800, 600);
    applyTestTheme(view);
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));

    DirSizeNodePtr root = buildTreeWithFramedDir();
    view.setRoot(root);

    DirSizeNode *big = root->children[0].get();
    QVERIFY(big && big->isDir);

    view.drillInto(big);
    QVERIFY2(view.isZoomRunning(),
             "drilling into a framed directory on a shown, non-reduced-motion view should animate");
    QCOMPARE(view.focus(), big);

    view.resize(700, 500);
    QVERIFY2(!view.isZoomRunning(), "resizing mid-animation must stop the zoom");
}

void TestTreemapZoom::emptyDirectory_doesNotAnimate()
{
    TreemapView view;
    view.resize(800, 600);
    applyTestTheme(view);
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));

    DirSizeNodePtr root = buildTreeWithEmptyDir();
    view.setRoot(root);

    DirSizeNode *empty = root->children[0].get();
    QVERIFY(empty && empty->isDir);
    QVERIFY(empty->children.empty());

    view.drillInto(empty);
    QVERIFY2(!view.isZoomRunning(),
             "drilling into a directory that lays out to nothing must not animate");
    QCOMPARE(view.focus(), empty);
}

QTEST_MAIN(TestTreemapZoom)
#include "test_treemap_zoom.moc"
