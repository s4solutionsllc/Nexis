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

} // namespace BubbleLayout

#endif // BUBBLE_LAYOUT_H
