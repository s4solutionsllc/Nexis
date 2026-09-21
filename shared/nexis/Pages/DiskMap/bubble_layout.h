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

Result build(DirSizeNode *focus, const QRectF &area, const Metrics &m = Metrics());
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
