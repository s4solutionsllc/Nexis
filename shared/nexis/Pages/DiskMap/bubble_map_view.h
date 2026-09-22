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

    /// Test seam: number of worker dispatches actually launched (not
    /// counting ones coalesced onto an already-in-flight request for the
    /// same key — see dispatchPackIfNeeded()).
    int packDispatchCount() const { return mPackDispatchCount; }

    /// Test seam: the colour paintGroup()/paintBubble() would use for
    /// `node` right now — against the hue snapshot frozen alongside
    /// mDisplayedTree/mLayout, not DiskMapView's live one (which may
    /// already have moved on to a newer, still-packing focus). See
    /// DiskMapView::colourFor()'s two-argument overload.
    QColor debugColourForDisplayedNode(DirSizeNode *node) const { return colourFor(node, mDisplayedHueSlots); }

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
    void dispatchPackIfNeeded(const QRectF &area, const BubbleLayout::Metrics &m);
    void startAsyncPack(const QRectF &area, const BubbleLayout::Metrics &m, const BubbleLayout::PackKey &key);
    void onPackFinished(QFutureWatcher<BubbleLayout::PackCache> *watcher, quint64 dispatchSeq, quint64 dispatchTreeGeneration);

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
    // The hue assignment mLayout's nodes were coloured with — see
    // DiskMapView::colourFor()'s two-argument overload. setRoot()/
    // drillInto()/drillUp() refresh the *live* hue map for the new focus
    // synchronously, before rebuildLayout() even runs, so while mLayout is
    // deliberately still showing an older focus's nodes (async pack
    // pending), the live map no longer has entries for them. Reassigned in
    // lockstep with mLayout, never independently.
    QHash<const DirSizeNode*, HueSlot> mDisplayedHueSlots;
    // Persists across resizes (only invalidated when the tree itself is
    // replaced, in rootAboutToChange()) so a resize/window-drag only pays
    // for the cheap affine fit, not the O(n^2) circle-packing relaxation.
    BubbleLayout::PackCache mPackCache;

    // Async pack (cold-cache circle-packing moved off the UI thread).
    //
    // mTreeGeneration is bumped only in rootAboutToChange() (i.e. only when
    // mRoot is about to be replaced) — a landed worker result whose
    // dispatch-time tree generation doesn't match the current one was
    // packed against a tree that either no longer exists from this view's
    // point of view or, worse, whose PackCache keys (raw DirSizeNode*) may
    // now collide with a *different* tree's node addresses (heap reuse), so
    // it must never be merged. This is deliberately a separate counter from
    // "which request is the freshest" (there is no such counter any more —
    // see below): a resize on the *same* tree must still benefit from an
    // in-flight pack even though something else has since asked for a
    // different focus/area.
    //
    // At most one worker is ever dispatched at a time for the same
    // (tree generation, PackKey) — see dispatchPackIfNeeded(). A resize
    // drag can walk many aspect buckets in a burst, each a genuine cache
    // miss; without this, every one of them would queue its own ~hundreds-
    // of-ms job on the thread pool (see bubblePackThreadPool() in the
    // .cpp), most of them wasted the instant a newer one supersedes them.
    // mPackDispatchSeq/mPackWorkerTrackedSeq identify whichever dispatch is
    // the most recently issued one so a superseded dispatch landing late
    // never clobbers the bookkeeping for a newer one still in flight.
    quint64 mTreeGeneration = 0;
    quint64 mPackDispatchSeq = 0;
    quint64 mPackWorkerTrackedSeq = 0;    ///< 0 == no worker currently tracked as in flight
    quint64 mPackWorkerTreeGeneration = 0;
    BubbleLayout::PackKey mPackWorkerKey;
    int mPackDispatchCount = 0;           ///< test seam only — see packDispatchCount()

    bool mPendingPack = false;
    bool mShowPackingMessage = false;
    QTimer *mPackDelayTimer = nullptr;
};

#endif // BUBBLE_MAP_VIEW_H
