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
    qreal minSweepDeg    = 0.4;
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
    bool placeholder = false; ///< ring-1 only: a file or empty directory's muted stand-in wedge
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
