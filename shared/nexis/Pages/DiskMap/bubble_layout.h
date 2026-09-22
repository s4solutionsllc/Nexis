// SSO-24963: pure-geometry nested bubble (circle-packing) layout, decoupled
// from painting — mirrors TreemapLayout.
//
// BubbleLayout::build() lays out one focus directory's children as
// area-proportional circles, packing any child directory large enough to
// hold a labelled membrane (a Group) with its own children packed inside,
// and flattening everything else (files, or directories too small to
// frame) into top-level leaf bubbles.

#ifndef BUBBLE_LAYOUT_H
#define BUBBLE_LAYOUT_H

#include <QHash>
#include <QPair>
#include <QPointF>
#include <QRectF>
#include <QVector>

#include "Managers/dir_size_scanner.h"

namespace BubbleLayout {

struct Metrics {
    qreal minGroupR  = 45;
    qreal innerPad   = 4;
    qreal labelBand  = 16;
    qreal minBubbleR = 1.5;
    qreal outerMargin = 4;
};

struct Bubble {
    QPointF center;
    qreal radius = 0;
    DirSizeNode *node = nullptr;
    int groupIndex = -1;
};

struct Group {
    QPointF center;
    qreal radius = 0;
    DirSizeNode *node = nullptr;
};

struct Result {
    QVector<Group> groups;
    QVector<Bubble> bubbles;
};

/// Caches the expensive, size-independent part of circle-packing (see
/// build()) across affine-only resizes. Packing is done once per tree node
/// in unit space (radii normalised so the largest is 1 — a pure function of
/// the node's children and their sizes) and reused; only the cheap fit-to-
/// area scale/translate is recomputed every call. Owned by the view; pass
/// nullptr to build() to disable caching (e.g. from tests that don't want
/// to reason about cache state). Never look up an entry keyed by a node
/// from a tree that has since been replaced — call clear() first.
class PackCache
{
public:
    void clear();

    /// Number of packs actually computed (cache misses) since construction
    /// or the last clear() — a running total, never reset by clear() itself
    /// (clearing just forces the *next* build() to recompute). Test seam:
    /// assert cache effectiveness via this counter, never via timing.
    int packCount() const { return mPackCount; }

    // Internal to BubbleLayout::build() — not part of the public contract.
    struct UnitPack {
        QVector<QPointF> pos;
        QRectF boundingBox;
        QPointF enclosingCenter;
        qreal enclosingR = 1.0;
    };
    const UnitPack &nestedPack(DirSizeNode *node, const QVector<qreal> &unitRadii);
    const UnitPack &topPack(DirSizeNode *node, const QVector<qreal> &unitRadii, qreal aspect);

private:
    QHash<const DirSizeNode*, UnitPack> mNested;
    QHash<QPair<const DirSizeNode*, int>, UnitPack> mTop;
    int mPackCount = 0;
};

Result build(DirSizeNode *focus, const QRectF &area, const Metrics &m = Metrics(), PackCache *cache = nullptr);
DirSizeNode *hitTest(const Result &r, const QPointF &pos);

/// The rectangle a group's "name · size" label should be drawn into —
/// positioned inside the membrane just below the rim (its vertical centre
/// sits roughly 3/4 of the way down the reserved label band, so the
/// baseline lands there too) and wide enough to fit its chord at that
/// height. Returns an empty QRectF (check with isEmpty()) when the group is
/// too small for the band to fit at all, or the chord there is under 60px —
/// the single source of truth both build()'s nested-pack room reservation
/// and BubbleMapView's paint use, so they can never disagree about whether
/// a label shows.
QRectF labelBandRect(const Group &g, const Metrics &m = Metrics());

} // namespace BubbleLayout

#endif // BUBBLE_LAYOUT_H
