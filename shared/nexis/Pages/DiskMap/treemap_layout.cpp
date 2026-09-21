#include "treemap_layout.h"

#include <algorithm>
#include <limits>

namespace {

using Placed = QVector<QPair<DirSizeNode*, QRectF>>;

// `row`'s sizes are raw byte counts, not pixel areas, so worstAspect must be
// given `scale` (pixel-area per byte, held constant for the whole squarify()
// call — see squarify()'s comment) to convert them before applying the
// Bruls et al. formula, which is only valid when values and width² share
// units.
qreal worstAspect(const QVector<DirSizeNode*> &row, qreal sum, qreal width, qreal scale)
{
    if (row.isEmpty() || sum <= 0 || width <= 0)
        return std::numeric_limits<qreal>::infinity();

    qreal maxVal = 0, minVal = std::numeric_limits<qreal>::max();
    for (auto *n : row) {
        const qreal area = n->size * scale;
        maxVal = std::max(maxVal, area);
        minVal = std::min(minVal, area);
    }
    // Treat zero-area items as a hair above zero so the divisor doesn't explode.
    if (minVal <= 0) minVal = std::numeric_limits<qreal>::min();

    const qreal areaSum = sum * scale;
    const qreal s2 = areaSum * areaSum;
    const qreal w2 = width * width;
    const qreal worst = std::max(w2 * maxVal / s2, s2 / (w2 * minVal));
    return worst;
}

void layoutRow(const QVector<DirSizeNode*> &row,
               qreal rowSum,
               qreal pendingValue,
               QRectF &remaining,
               Placed &out)
{
    if (rowSum <= 0 || row.isEmpty() || pendingValue <= 0)
        return;

    const bool horizontalSlab = remaining.width() >= remaining.height();
    const qreal share = rowSum / pendingValue;

    if (horizontalSlab) {
        // Strip eats from the left; height = remaining.height(); width =
        // share × remaining.width() so its area is `share × remaining.area`.
        qreal stripW = remaining.width() * share;
        stripW = std::min(stripW, remaining.width());
        QRectF strip(remaining.x(), remaining.y(), stripW, remaining.height());

        qreal y = strip.y();
        for (int idx = 0; idx < row.size(); ++idx) {
            DirSizeNode *n = row[idx];
            const qreal h = (idx == row.size() - 1)
                ? strip.bottom() - y
                : strip.height() * (static_cast<qreal>(n->size) / rowSum);
            out.append({n, QRectF(strip.x(), y, strip.width(), h)});
            y += h;
        }
        remaining = QRectF(strip.right(), remaining.y(),
                           remaining.width() - strip.width(),
                           remaining.height());
    } else {
        qreal stripH = remaining.height() * share;
        stripH = std::min(stripH, remaining.height());
        QRectF strip(remaining.x(), remaining.y(), remaining.width(), stripH);

        qreal x = strip.x();
        for (int idx = 0; idx < row.size(); ++idx) {
            DirSizeNode *n = row[idx];
            const qreal w = (idx == row.size() - 1)
                ? strip.right() - x
                : strip.width() * (static_cast<qreal>(n->size) / rowSum);
            out.append({n, QRectF(x, strip.y(), w, strip.height())});
            x += w;
        }
        remaining = QRectF(remaining.x(), strip.bottom(),
                           remaining.width(),
                           remaining.height() - strip.height());
    }
}

// Squarified treemap (Bruls, Huijsing, van Wijk 2000). `pendingValue` is the
// sum of the values that still need to fit into `rect`; every recursive call
// keeps the invariant that rect.area() corresponds exactly to pendingValue
// at the current scale, which is what makes the strip math come out clean.
void squarify(const QVector<DirSizeNode*> &items,
              QRectF rect,
              qreal pendingValue,
              Placed &out)
{
    if (items.isEmpty() || rect.width() <= 1 || rect.height() <= 1 ||
        pendingValue <= 0)
        return;

    // rect.area() corresponds to pendingValue at the current scale (see the
    // function comment above), so this ratio is constant for the whole call
    // and converts raw byte sizes into the pixel areas worstAspect needs.
    const qreal scale = rect.width() * rect.height() / pendingValue;

    QVector<DirSizeNode*> row;
    qreal rowSum = 0;
    qreal shortSide = std::min(rect.width(), rect.height());

    int i = 0;
    while (i < items.size()) {
        DirSizeNode *cand = items[i];
        QVector<DirSizeNode*> withCand = row;
        withCand.append(cand);
        const qreal candSum = rowSum + cand->size;

        const qreal worstBefore = worstAspect(row, rowSum, shortSide, scale);
        const qreal worstAfter  = worstAspect(withCand, candSum, shortSide, scale);

        if (row.isEmpty() || worstAfter <= worstBefore) {
            row = std::move(withCand);
            rowSum = candSum;
            ++i;
        } else {
            layoutRow(row, rowSum, pendingValue, rect, out);
            pendingValue -= rowSum;
            row.clear();
            rowSum = 0;
            shortSide = std::min(rect.width(), rect.height());
        }
    }
    if (!row.isEmpty())
        layoutRow(row, rowSum, pendingValue, rect, out);
}

Placed place(DirSizeNode *parent, const QRectF &rect)
{
    Placed out;
    QVector<DirSizeNode*> kids;
    qreal total = 0;
    for (auto &c : parent->children)
        if (c->size > 0) { kids.append(c.get()); total += c->size; }
    if (kids.isEmpty() || rect.width() <= 1 || rect.height() <= 1)
        return out;
    std::sort(kids.begin(), kids.end(),
              [](DirSizeNode *a, DirSizeNode *b) { return a->size > b->size; });
    squarify(kids, rect, total, out);
    return out;
}

QRectF inset(const QRectF &r, qreal g)
{
    const qreal dx = std::min(g / 2, r.width() / 4), dy = std::min(g / 2, r.height() / 4);
    return r.adjusted(dx, dy, -dx, -dy);
}

// A slab-based squarify can hand an extreme-disparity leaf (e.g. 999:1
// against its sibling) a proportionally-correct but very thin strip. Judge
// drawability by area rather than requiring both dimensions individually
// clear minTile, so such a leaf still yields a hit-testable tile.
bool isDrawable(const QRectF &r, qreal minTile)
{
    return r.width() > 0 && r.height() > 0 && r.width() * r.height() >= minTile * minTile;
}

} // namespace

TreemapLayout::Result TreemapLayout::build(DirSizeNode *focus, const QRectF &area, const Metrics &m)
{
    Result res;
    if (!focus || !focus->isDir || focus->size <= 0)
        return res;

    for (const auto &top : place(focus, area)) {
        DirSizeNode *node = top.first;
        const QRectF outer = inset(top.second, m.frameGap);
        const bool framed = node->isDir && !node->children.empty()
                            && outer.width() >= m.minFrameW && outer.height() >= m.minFrameH;
        if (!framed) {
            if (isDrawable(outer, m.minTile))
                res.tiles.append({outer, node, -1});
            continue;
        }
        Frame f;
        f.node = node;
        f.outer = outer;
        f.header = QRectF(outer.x(), outer.y(), outer.width(), m.headerH);
        f.content = outer.adjusted(m.tileGap, m.headerH, -m.tileGap, -m.tileGap);
        const int idx = res.frames.size();
        res.frames.append(f);
        for (const auto &kid : place(node, f.content)) {
            const QRectF r = inset(kid.second, m.tileGap);
            if (isDrawable(r, m.minTile))
                res.tiles.append({r, kid.first, idx});
        }
    }
    return res;
}

DirSizeNode *TreemapLayout::hitTest(const Result &r, const QPointF &pos)
{
    for (const Tile &t : r.tiles)
        if (t.rect.contains(pos))
            return t.node;
    for (const Frame &f : r.frames)
        if (f.outer.contains(pos))
            return f.node;
    return nullptr;
}
