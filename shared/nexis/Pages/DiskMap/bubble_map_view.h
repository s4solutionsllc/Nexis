// SSO-23862 / SSO-24963: bubble-map (nested circle-packing) visualization
// for the disk-space visualizer.
//
// Two nested levels, lit shapes, hover lift, and a drill cross-fade — mirrors
// the treemap redesign. Geometry lives in BubbleLayout (SSO-24963); this
// class only paints it and hit-tests it. Tree/focus/drill-stack/theme/hover/
// context-menu are owned by DiskMapView (shared with TreemapView and
// SunburstView).

#ifndef BUBBLE_MAP_VIEW_H
#define BUBBLE_MAP_VIEW_H

#include <QFutureWatcher>

#include "bubble_layout.h"
#include "disk_map_view.h"

class QTimer;

class BubbleMapView : public DiskMapView
{
    Q_OBJECT

public:
    explicit BubbleMapView(QWidget *parent = nullptr);

    /// Test seam: true while a background circle-pack for the current
    /// request (see rebuildLayout()) is still in flight.
    bool isPackPending() const { return mPendingPack; }

    /// Test seams: the geometry currently applied, and the running total of
    /// cache misses actually computed — see PackCache::packCount().
    const BubbleLayout::Result &layout() const { return mLayout; }
    int packCount() const { return mPackCache.packCount(); }

protected:
    void rebuildLayout() override;
    void paintEvent(QPaintEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void aboutToDrill(DirSizeNode *target, bool drillingIn) override;
    void rootAboutToChange() override;

private:
    BubbleLayout::Metrics scaledMetrics() const;
    void paintGroup(QPainter &p, const BubbleLayout::Group &g, bool hovered);
    void paintBubble(QPainter &p, const BubbleLayout::Bubble &b, bool hovered);
    void paintLayout(QPainter &p, const BubbleLayout::Result &layout);
    void startAsyncPack(const QRectF &area, const BubbleLayout::Metrics &m, quint64 generation);
    void onPackFinished(QFutureWatcher<BubbleLayout::PackCache> *watcher, quint64 generation);

    BubbleLayout::Result mLayout;
    // Keeps whatever tree mLayout's raw DirSizeNode* pointers actually
    // belong to alive, independent of mRoot. Async packing means mLayout
    // can still be showing the *previous* request's geometry (deliberately
    // — see rebuildLayout()) after a later setRoot() has already replaced
    // mRoot with a new tree and let this view's own share of the old one
    // go; without a second owner here, that old tree would be freed out
    // from under mLayout the moment nothing else references it. Reassigned
    // in lockstep with mLayout, never independently.
    DirSizeNodePtr mDisplayedTree;
    // Persists across resizes (only invalidated when the tree itself is
    // replaced, in rootAboutToChange()) so a resize/window-drag only pays
    // for the cheap affine fit, not the O(n^2) circle-packing relaxation.
    BubbleLayout::PackCache mPackCache;

    // Async pack (cold-cache circle-packing moved off the UI thread).
    // mPackGeneration is bumped on every rebuildLayout() request (drill,
    // resize, setRoot) so a QFutureWatcher::finished handler that fires
    // after a later request has superseded it can tell its result is stale
    // and must be discarded rather than merged into mPackCache/applied to
    // mLayout — see onPackFinished().
    quint64 mPackGeneration = 0;
    bool mPendingPack = false;
    bool mShowPackingMessage = false;
    QTimer *mPackDelayTimer = nullptr;
};

#endif // BUBBLE_MAP_VIEW_H
