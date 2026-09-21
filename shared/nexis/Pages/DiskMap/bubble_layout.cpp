#include "bubble_layout.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace {

// Circle-packing budget: the relaxation below is O(n^2 * iterations), so any
// single level (top-level children, or one group's members) is capped at the
// 150 largest items — plenty for what a disk map actually needs to show, and
// the rest (already the smallest, least visually significant) are simply
// dropped rather than degrading everyone's layout.
constexpr int kMaxPackedPerLevel = 150;

struct PackResult {
    QVector<QPointF> pos;   ///< positions around the origin, unscaled
    qreal boundingR = 1.0;  ///< radius of the smallest circle around the origin containing every packed circle
};

// Packs `radii` (already sized ∝ sqrt(value), any common scale) into
// non-overlapping circles around the origin. Golden-angle seed + pairwise
// separation relaxation + a final overlap-resolution pass so the "no more
// than 1px overlap" contract holds even for adversarial inputs, followed by
// a gentle centroid pull to keep the cluster tight. Approximate but visually
// snug, and cheap enough to run per group per rebuildLayout().
PackResult pack(const QVector<qreal> &radii)
{
    PackResult out;
    const int n = radii.size();
    if (n == 0)
        return out;
    if (n == 1) {
        out.pos = {QPointF(0, 0)};
        out.boundingR = std::max<qreal>(radii[0], 1e-6);
        return out;
    }

    QVector<QPointF> pos(n);
    const qreal golden = 137.50776405 * M_PI / 180.0;
    qreal spread = 0;
    for (qreal r : radii)
        spread += r;
    spread = std::max(spread, 1.0);
    for (int i = 0; i < n; ++i) {
        const qreal a = i * golden;
        const qreal radius = spread * std::sqrt(static_cast<qreal>(i + 1) / n);
        pos[i] = QPointF(radius * std::cos(a), radius * std::sin(a));
    }

    // More iterations for small n (cheap, and small groups benefit most from
    // a tight pack); still bounded for large n by the kMaxPackedPerLevel cap.
    const int iterations = n <= 20 ? 400 : 200;
    for (int iter = 0; iter < iterations; ++iter) {
        for (int i = 0; i < n; ++i)
            pos[i] -= pos[i] * 0.02;

        for (int i = 0; i < n; ++i) {
            for (int j = i + 1; j < n; ++j) {
                QPointF d = pos[j] - pos[i];
                const qreal dist = std::hypot(d.x(), d.y());
                const qreal minDist = radii[i] + radii[j];
                if (dist < minDist) {
                    const qreal overlap = minDist - dist;
                    const QPointF dir = dist > 1e-6 ? d / dist : QPointF(1, 0);
                    pos[i] -= dir * (overlap * 0.5);
                    pos[j] += dir * (overlap * 0.5);
                }
            }
        }
    }

    // Final deterministic overlap-resolution pass: push apart any pair still
    // closer than their radii sum, no centroid pull, so this always
    // converges to zero (meaningful) overlap rather than trading it for
    // drift.
    for (int pass = 0; pass < 40; ++pass) {
        bool any = false;
        for (int i = 0; i < n; ++i) {
            for (int j = i + 1; j < n; ++j) {
                QPointF d = pos[j] - pos[i];
                const qreal dist = std::hypot(d.x(), d.y());
                const qreal minDist = radii[i] + radii[j];
                if (dist < minDist - 0.01) {
                    any = true;
                    const qreal overlap = minDist - dist;
                    const QPointF dir = dist > 1e-6 ? d / dist : QPointF(1, 0);
                    pos[i] -= dir * (overlap * 0.5);
                    pos[j] += dir * (overlap * 0.5);
                }
            }
        }
        if (!any)
            break;
    }

    qreal boundingR = 1.0;
    for (int i = 0; i < n; ++i)
        boundingR = std::max(boundingR, std::hypot(pos[i].x(), pos[i].y()) + radii[i]);

    out.pos = pos;
    out.boundingR = boundingR;
    return out;
}

// Largest-`kMaxPackedPerLevel` items by size, sorted descending. Mirrors
// TreemapLayout::place()'s child selection (size > 0 only).
QVector<DirSizeNode*> selectAndSort(DirSizeNode *parent)
{
    QVector<DirSizeNode*> kids;
    for (auto &c : parent->children)
        if (c->size > 0)
            kids.append(c.get());
    std::sort(kids.begin(), kids.end(),
              [](DirSizeNode *a, DirSizeNode *b) { return a->size > b->size; });
    if (kids.size() > kMaxPackedPerLevel)
        kids.resize(kMaxPackedPerLevel);
    return kids;
}

// Whether a label band of height `labelBand` at the top of a disc of radius
// `discR` is wide enough (chord ≥ 60px) to actually show a label — shared
// between layout (does the nested pack need to leave room?) and painting
// (does the membrane get a label drawn?).
bool bandFitsLabel(qreal discR, qreal labelBand)
{
    if (discR <= labelBand)
        return false;
    const qreal half = discR - labelBand;
    const qreal chord = 2.0 * std::sqrt(std::max<qreal>(0, discR * discR - half * half));
    return chord >= 60.0;
}

} // namespace

BubbleLayout::Result BubbleLayout::build(DirSizeNode *focus, const QRectF &area, const Metrics &m)
{
    Result res;
    if (!focus || !focus->isDir || focus->size <= 0)
        return res;
    if (area.width() <= 0 || area.height() <= 0)
        return res;

    QVector<DirSizeNode*> tops = selectAndSort(focus);
    if (tops.isEmpty())
        return res;

    QVector<qreal> radii(tops.size());
    for (int i = 0; i < tops.size(); ++i)
        radii[i] = std::sqrt(static_cast<qreal>(tops[i]->size));

    const PackResult packed = pack(radii);

    const QRectF usable = area.adjusted(m.outerMargin, m.outerMargin, -m.outerMargin, -m.outerMargin);
    if (usable.width() <= 0 || usable.height() <= 0)
        return res;
    const qreal targetR = std::min(usable.width(), usable.height()) / 2.0;
    const qreal scale = targetR / std::max(packed.boundingR, 1e-6);
    const QPointF center = usable.center();

    for (int i = 0; i < tops.size(); ++i) {
        DirSizeNode *node = tops[i];
        const QPointF finalCenter = center + packed.pos[i] * scale;
        const qreal finalRadius = radii[i] * scale;

        const bool wantsGroup = node->isDir && !node->children.empty() && finalRadius >= m.minGroupR;
        if (!wantsGroup) {
            if (finalRadius >= m.minBubbleR)
                res.bubbles.append({finalCenter, finalRadius, node, -1});
            continue;
        }

        Group g;
        g.center = finalCenter;
        g.radius = finalRadius;
        g.node = node;
        const int groupIndex = res.groups.size();
        res.groups.append(g);

        QVector<DirSizeNode*> kids = selectAndSort(node);
        if (kids.isEmpty())
            continue;

        qreal discR = std::max<qreal>(0, finalRadius - m.innerPad);
        QPointF discCenter = finalCenter;
        if (bandFitsLabel(discR, m.labelBand)) {
            discR = std::max<qreal>(0, discR - m.labelBand / 2.0);
            discCenter += QPointF(0, m.labelBand / 2.0);
        }
        if (discR <= 0)
            continue;

        QVector<qreal> kidRadii(kids.size());
        for (int j = 0; j < kids.size(); ++j)
            kidRadii[j] = std::sqrt(static_cast<qreal>(kids[j]->size));
        const PackResult kidPacked = pack(kidRadii);
        const qreal kidScale = discR / std::max(kidPacked.boundingR, 1e-6);

        for (int j = 0; j < kids.size(); ++j) {
            const qreal r = kidRadii[j] * kidScale;
            if (r < m.minBubbleR)
                continue;
            res.bubbles.append({discCenter + kidPacked.pos[j] * kidScale, r, kids[j], groupIndex});
        }
    }

    return res;
}

DirSizeNode *BubbleLayout::hitTest(const Result &r, const QPointF &pos)
{
    for (const Bubble &b : r.bubbles) {
        const QPointF d = pos - b.center;
        if (std::hypot(d.x(), d.y()) <= b.radius)
            return b.node;
    }
    for (const Group &g : r.groups) {
        const QPointF d = pos - g.center;
        if (std::hypot(d.x(), d.y()) <= g.radius)
            return g.node;
    }
    return nullptr;
}
