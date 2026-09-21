// SSO-24963: pure-geometry nested treemap layout, decoupled from painting.
//
// TreemapLayout::build() lays out one focus directory's children as a
// squarified treemap, framing any child directory large enough to hold a
// header + nested content area, and flattening everything else (files, or
// directories too small to frame) into top-level leaf tiles.

#ifndef TREEMAP_LAYOUT_H
#define TREEMAP_LAYOUT_H

#include <QPointF>
#include <QRectF>
#include <QVector>

#include "Managers/dir_size_scanner.h"

namespace TreemapLayout {

struct Metrics {
    qreal frameGap = 6;
    qreal tileGap = 3;
    qreal headerH = 18;
    qreal minFrameW = 90;
    qreal minFrameH = 60;
    qreal minTile = 3;
};

struct Tile {
    QRectF rect;
    DirSizeNode *node = nullptr;
    int frameIndex = -1;
};

struct Frame {
    QRectF outer, header, content;
    DirSizeNode *node = nullptr;
};

struct Result {
    QVector<Frame> frames;
    QVector<Tile> tiles;
};

Result build(DirSizeNode *focus, const QRectF &area, const Metrics &m = Metrics());
DirSizeNode *hitTest(const Result &r, const QPointF &pos);

} // namespace TreemapLayout

#endif // TREEMAP_LAYOUT_H
