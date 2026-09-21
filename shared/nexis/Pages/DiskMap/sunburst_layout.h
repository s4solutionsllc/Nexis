// SSO-24963: pure-geometry two-ring sunburst layout, decoupled from painting
// — mirrors TreemapLayout/BubbleLayout.
//
// SunburstLayout::build() lays out one focus directory's children as a ring
// of wedges (ring 0), sweep proportional to size, and subdivides each ring-0
// directory wedge's angular span with its own children (ring 1). A ring-0
// file, or a directory with no size>0 children, gets a single desaturated
// "placeholder" ring-1 wedge spanning the same arc and resolving to the same
// node, so hover/drill/tooltip always land on something meaningful.
//
// Items whose wedge would be imperceptibly thin (below Metrics::minArcPx at
// its ring's mid-radius, or below Metrics::minSweepDeg as an absolute floor)
// are not drawn individually — since siblings are processed size-descending
// and arc length is monotonic in size at a fixed radius, they always form a
// contiguous trailing run, so they're folded into a single trailing
// "remainder" wedge covering the rest of their parent's span. A remainder's
// node is the *parent* (the ring-0 directory for a ring-1 remainder, or
// `focus` itself for a ring-0 remainder), so hover/drill/tooltip land on
// something real instead of a blank gap. Marked `remainder` (painted muted,
// like `placeholder`) so callers can tell the two apart when they need to.
//
// QPainterPath (QtGui) deliberately does not appear here: nexis-core (which
// this header's test target links) only pulls in Qt6::Core/Network/
// Concurrent, and every other *_layout.h in this directory is QtGui-free
// too. Wedge-to-path conversion lives in SunburstView instead.

#ifndef SUNBURST_LAYOUT_H
#define SUNBURST_LAYOUT_H

#include <QPointF>
#include <QRectF>
#include <QVector>

#include "Managers/dir_size_scanner.h"

namespace SunburstLayout {

struct Metrics {
    qreal hubRatio       = 0.32;
    qreal innerRingRatio = 0.68;
    qreal ringGap        = 3;
    qreal wedgeGapPx      = 1.5;
    qreal minSweepDeg    = 0.4;   ///< absolute floor, regardless of radius
    qreal minArcPx       = 6;     ///< arc length at the ring's mid-radius below which a wedge is folded into its parent's remainder
    qreal margin         = 8;
};

struct Wedge {
    qreal startDeg = 0;   ///< clockwise from 12 o'clock, degrees
    qreal sweepDeg = 0;
    qreal innerR = 0;
    qreal outerR = 0;
    DirSizeNode *node = nullptr;
    int ring = 0;          ///< 0 or 1
    int parentIndex = -1;  ///< index into Result::wedges of the ring-0 parent (ring-1 only)
    bool placeholder = false; ///< a file or empty directory's muted stand-in wedge (ring 1) or an aggregated-remainder's own muted ring-1 band (ring 1)
    bool remainder = false;   ///< a trailing aggregate of siblings too thin to draw individually; node is their parent
};

struct Result {
    QPointF center;
    qreal hubR = 0;
    qreal outerR = 0;
    qreal ring0InnerR = 0, ring0OuterR = 0;
    qreal ring1InnerR = 0, ring1OuterR = 0;
    QVector<Wedge> wedges;
};

Result build(DirSizeNode *focus, const QRectF &area, const Metrics &m = Metrics());
DirSizeNode *hitTest(const Result &r, const QPointF &pos);

} // namespace SunburstLayout

#endif // SUNBURST_LAYOUT_H
