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

    /// Number of packs this cache actually holds work for — computed here
    /// directly (a cache miss during nestedPack()/topPack()) or folded in
    /// from a worker-thread cache via mergeFrom() (still real, newly-
    /// obtained work from this cache's point of view, just computed
    /// elsewhere) — since construction or the last clear(). A running
    /// total, never reset by clear() itself (clearing just forces the
    /// *next* build() to recompute). Test seam: assert cache effectiveness
    /// via this counter, never via timing.
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

    /// Read-only peeks at an already-cached pack — nullptr on a miss, never
    /// computing or inserting anything. The single source of truth for
    /// whether a given key is cached, shared by packsCached() (the
    /// sync/async decision) and by nestedPack()/topPack() themselves via
    /// aspectBucket(), so the two can never quantise a key differently and
    /// disagree about the same entry.
    const UnitPack *tryNestedPack(const DirSizeNode *node) const;
    const UnitPack *tryTopPack(const DirSizeNode *node, qreal aspect) const;

    /// Copies in any pack from `other` this cache doesn't already have.
    /// GUI-thread only: `other` is a worker-thread-computed cache (see
    /// BubbleMapView's async pack path) being folded back in after the
    /// fact — never call this with a cache another thread might still be
    /// touching, and never call it from that worker thread.
    void mergeFrom(const PackCache &other);

    /// The 0.05-quantised aspect bucket topPack()/tryTopPack() key on.
    /// Public so packKeyFor() below (and anything else identifying a
    /// top-level pack request from the outside, e.g. BubbleMapView's
    /// in-flight-worker dedup) can derive the exact same bucket rather
    /// than re-deriving its own quantisation that could drift from this
    /// one.
    static int aspectBucket(qreal aspect);

private:
    QHash<const DirSizeNode*, UnitPack> mNested;
    QHash<QPair<const DirSizeNode*, int>, UnitPack> mTop;
    int mPackCount = 0;
};

Result build(DirSizeNode *focus, const QRectF &area, const Metrics &m = Metrics(), PackCache *cache = nullptr);
DirSizeNode *hitTest(const Result &r, const QPointF &pos);

/// True iff build() with these exact arguments would be a pure cache hit
/// (affine fit only, no circle-packing relaxation) — used to decide whether
/// BubbleMapView can lay out synchronously or must hand the miss to a
/// worker thread. Walks the same top-level/group selection build() does,
/// but only *asks* the cache (via PackCache::tryTopPack()/tryNestedPack())
/// instead of computing anything, so it can never do the expensive work
/// itself. Always returns true when there is nothing to pack (null/empty
/// focus, degenerate area) and always false when `cache` is null, since
/// there is then nothing to have cached.
bool packsCached(DirSizeNode *focus, const QRectF &area, const Metrics &m = Metrics(), const PackCache *cache = nullptr);

/// Identifies one build() request's top-level pack — the exact key
/// PackCache::topPack() would use internally (same node, same quantised
/// aspect bucket — see PackCache::aspectBucket()). Two requests with an
/// equal PackKey will always look up (or, on a miss, compute) the same
/// cache entry. Used by BubbleMapView to recognise a request that's
/// already covered by an in-flight worker so it doesn't dispatch a
/// redundant one (e.g. a resize drag re-entering the same aspect bucket).
struct PackKey {
    const DirSizeNode *node = nullptr;
    int aspectBucket = 0;

    bool operator==(const PackKey &other) const
    {
        return node == other.node && aspectBucket == other.aspectBucket;
    }
};
PackKey packKeyFor(DirSizeNode *focus, const QRectF &area, const Metrics &m = Metrics());

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
