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
    QVector<QPointF> pos;      ///< final positions, relative to the seed origin
    QRectF boundingBox;        ///< tight envelope of every packed circle (center +- radius)
    QPointF enclosingCenter;   ///< bounding-box centre — also the (approximate) minimal enclosing circle's centre
    qreal enclosingR = 1.0;    ///< radius of the enclosing circle around enclosingCenter (classic "box centre, then max(dist+r)" construction)
};

void separationSweep(QVector<QPointF> &pos, const QVector<qreal> &radii)
{
    const int n = pos.size();
    for (int i = 0; i < n; ++i) {
        for (int j = i + 1; j < n; ++j) {
            QPointF d = pos[j] - pos[i];
            const qreal dist = std::sqrt(d.x() * d.x() + d.y() * d.y());
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

// Deterministic overlap-resolution: push apart any pair still closer than
// their radii sum, no centroid pull, repeated until nothing overlaps (or
// `maxPasses` runs out) so this always converges to zero (meaningful)
// overlap rather than trading it for drift. Used both right after the
// coarse relaxation and again after compaction, since compaction's larger
// pull coefficient can reintroduce overlap that its own fixed 5-sweep
// budget doesn't always fully clear. The tolerance is unit-space (radii are
// always normalised to max == 1, see build()) — 1e-4 stays well under 1px
// once scaled back up to any plausible on-screen radius.
void resolveOverlaps(QVector<QPointF> &pos, const QVector<qreal> &radii, int maxPasses)
{
    const int n = pos.size();
    for (int pass = 0; pass < maxPasses; ++pass) {
        bool any = false;
        for (int i = 0; i < n; ++i) {
            for (int j = i + 1; j < n; ++j) {
                QPointF d = pos[j] - pos[i];
                const qreal dist = std::sqrt(d.x() * d.x() + d.y() * d.y());
                const qreal minDist = radii[i] + radii[j];
                if (dist < minDist - 1e-4) {
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
}

// Packs `radii` into non-overlapping circles. Callers always pass unit-space
// radii (largest == 1, see build()'s normalisation) so the convergence
// threshold below is a meaningful fraction of the cluster's own scale
// regardless of how large the underlying byte counts are — packing is
// self-similar (homogeneous of degree 1 in the radii), so running it in
// unit space and scaling the result afterwards is equivalent to running it
// at the true scale, and immune to that scale's magnitude.
//
// Golden-angle seed + pairwise separation relaxation (pulled anisotropically
// toward `aspect` = target width/height so a wide destination rect gets a
// wide cluster instead of a disc) + a deterministic overlap-resolution pass,
// then a compaction phase that repeatedly pulls every circle toward the
// cluster centroid and resolves the overlap that creates, until movement
// drops below 1e-3 (unit space) or the (small but generous for n < 20)
// iteration budget runs out — this is what actually closes the visible gaps
// a coarse relaxation leaves between neighbouring circles. `aspect` only
// biases the first relaxation; the compaction phase re-applies it too so the
// horizontal spread it bought doesn't get pulled back into a disc while
// tightening gaps.
PackResult pack(const QVector<qreal> &radii, qreal aspect = 1.0)
{
    PackResult out;
    const int n = radii.size();
    if (n == 0)
        return out;

    aspect = std::clamp(aspect, 0.1, 10.0);
    const qreal pullX = 0.02 / aspect;
    const qreal pullY = 0.02 * aspect;

    if (n == 1) {
        out.pos = {QPointF(0, 0)};
        out.boundingBox = QRectF(-radii[0], -radii[0], radii[0] * 2, radii[0] * 2);
        out.enclosingCenter = QPointF(0, 0);
        out.enclosingR = std::max<qreal>(radii[0], 1e-6);
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
        for (int i = 0; i < n; ++i) {
            pos[i].rx() -= pos[i].x() * pullX;
            pos[i].ry() -= pos[i].y() * pullY;
        }
        separationSweep(pos, radii);
    }

    resolveOverlaps(pos, radii, 40);

    // Compaction: closes the gaps the coarse relaxation above leaves between
    // neighbours by continuing the same proportional centroid pull + several
    // separation-sweep passes (a single pairwise sweep doesn't fully resolve
    // a chain of 3+ circles the pull just crowded together) at a much larger
    // pull coefficient — the cluster is already roughly packed by now, so a
    // big pull mostly resolves through separation rather than distorting the
    // shape — until the cluster stops moving (< 1e-3, unit space) or the
    // iteration budget (generous for the small-n case this matters most for)
    // runs out. Proportional (not normalized-and-capped) so it keeps the same
    // aspect bias as the relaxation above instead of drifting back toward a
    // disc.
    const int compactIters = n <= 20 ? 3000 : (n <= 60 ? 800 : 200);
    const qreal compactPullX = 0.25 / aspect;
    const qreal compactPullY = 0.25 * aspect;
    for (int iter = 0; iter < compactIters; ++iter) {
        QPointF centroid(0, 0);
        for (const auto &pt : pos)
            centroid += pt;
        centroid /= n;

        qreal maxMove = 0;
        for (int i = 0; i < n; ++i) {
            const QPointF before = pos[i];
            pos[i].rx() -= (pos[i].x() - centroid.x()) * compactPullX;
            pos[i].ry() -= (pos[i].y() - centroid.y()) * compactPullY;
            maxMove = std::max(maxMove, std::hypot(pos[i].x() - before.x(), pos[i].y() - before.y()));
        }
        for (int sp = 0; sp < 5; ++sp)
            separationSweep(pos, radii);

        if (maxMove < 1e-3)
            break;
    }

    // Compaction's larger pull can reintroduce overlap its own fixed sweep
    // budget doesn't always fully clear within the loop above — guarantee
    // the "no overlap" contract holds regardless.
    resolveOverlaps(pos, radii, 60);

    qreal minX = std::numeric_limits<qreal>::max(), maxX = std::numeric_limits<qreal>::lowest();
    qreal minY = std::numeric_limits<qreal>::max(), maxY = std::numeric_limits<qreal>::lowest();
    for (int i = 0; i < n; ++i) {
        minX = std::min(minX, pos[i].x() - radii[i]);
        maxX = std::max(maxX, pos[i].x() + radii[i]);
        minY = std::min(minY, pos[i].y() - radii[i]);
        maxY = std::max(maxY, pos[i].y() + radii[i]);
    }
    const QRectF box(QPointF(minX, minY), QPointF(maxX, maxY));
    const QPointF boxCenter = box.center();

    qreal enclosingR = 1e-6;
    for (int i = 0; i < n; ++i) {
        const QPointF d = pos[i] - boxCenter;
        enclosingR = std::max(enclosingR, std::hypot(d.x(), d.y()) + radii[i]);
    }

    out.pos = pos;
    out.boundingBox = box;
    out.enclosingCenter = boxCenter;
    out.enclosingR = enclosingR;
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

BubbleLayout::PackCache::UnitPack toUnitPack(const PackResult &pr)
{
    BubbleLayout::PackCache::UnitPack up;
    up.pos = pr.pos;
    up.boundingBox = pr.boundingBox;
    up.enclosingCenter = pr.enclosingCenter;
    up.enclosingR = pr.enclosingR;
    return up;
}

} // namespace

void BubbleLayout::PackCache::clear()
{
    mNested.clear();
    mTop.clear();
}

const BubbleLayout::PackCache::UnitPack &BubbleLayout::PackCache::nestedPack(DirSizeNode *node, const QVector<qreal> &unitRadii)
{
    auto it = mNested.constFind(node);
    if (it != mNested.constEnd())
        return it.value();
    ++mPackCount;
    // Nested packs go into a circular disc, so no aspect bias here.
    return mNested.insert(node, toUnitPack(pack(unitRadii))).value();
}

const BubbleLayout::PackCache::UnitPack &BubbleLayout::PackCache::topPack(DirSizeNode *node, const QVector<qreal> &unitRadii, qreal aspect)
{
    const int bucket = int(std::lround(aspect / 0.05));
    const auto key = qMakePair(static_cast<const DirSizeNode*>(node), bucket);
    auto it = mTop.constFind(key);
    if (it != mTop.constEnd())
        return it.value();
    ++mPackCount;
    return mTop.insert(key, toUnitPack(pack(unitRadii, aspect))).value();
}

QRectF BubbleLayout::labelBandRect(const Group &g, const Metrics &m)
{
    if (g.radius <= m.labelBand)
        return QRectF();

    // Baseline sits 3/4 of the way down the band from the rim, so the
    // glyphs' visual weight (mostly above the baseline) centers inside the
    // band instead of poking through the rim — check the chord at that
    // height, not the (wider, and therefore unsafe) chord at the band's
    // inner edge.
    const qreal baselineDy = g.radius - m.labelBand * 0.75;
    const qreal chord = 2.0 * std::sqrt(std::max<qreal>(0, g.radius * g.radius - baselineDy * baselineDy));
    if (chord < 60.0)
        return QRectF();

    const qreal baselineY = g.center.y() - baselineDy;
    const qreal h = m.labelBand * 0.9;
    return QRectF(g.center.x() - chord / 2.0, baselineY - h / 2.0, chord, h);
}

BubbleLayout::Result BubbleLayout::build(DirSizeNode *focus, const QRectF &area, const Metrics &m, PackCache *cache)
{
    Result res;
    if (!focus || !focus->isDir || focus->size <= 0)
        return res;
    if (area.width() <= 0 || area.height() <= 0)
        return res;

    QVector<DirSizeNode*> tops = selectAndSort(focus);
    if (tops.isEmpty())
        return res;

    // Pack in unit space (largest radius == 1) — see pack()'s comment for
    // why this is equivalent to packing at true scale. `radii` (true scale,
    // ∝ sqrt(bytes)) is only needed afterwards to derive `unitRadii`; the
    // final geometry below is independent of it (maxRadius cancels out).
    QVector<qreal> radii(tops.size());
    qreal maxRadius = 0;
    for (int i = 0; i < tops.size(); ++i) {
        radii[i] = std::sqrt(static_cast<qreal>(tops[i]->size));
        maxRadius = std::max(maxRadius, radii[i]);
    }
    maxRadius = std::max(maxRadius, 1e-9);
    QVector<qreal> unitRadii(tops.size());
    for (int i = 0; i < tops.size(); ++i)
        unitRadii[i] = radii[i] / maxRadius;

    const QRectF usable = area.adjusted(m.outerMargin, m.outerMargin, -m.outerMargin, -m.outerMargin);
    if (usable.width() <= 0 || usable.height() <= 0)
        return res;

    const qreal aspect = usable.width() / std::max(usable.height(), 1e-6);

    PackCache::UnitPack localTop;
    if (!cache)
        localTop = toUnitPack(pack(unitRadii, aspect));
    const PackCache::UnitPack &top = cache ? cache->topPack(focus, unitRadii, aspect) : localTop;

    // Fit the packed cluster's true bounding BOX (not a bounding circle
    // around its seed origin) into the usable rect, limited by whichever
    // dimension is tighter, so the cluster fills the card instead of
    // floating as an undersized disc in the middle of it.
    const QRectF unitBox = top.boundingBox;
    const qreal fitScale = std::min(usable.width() / std::max(unitBox.width(), 1e-6),
                                    usable.height() / std::max(unitBox.height(), 1e-6));
    const QPointF unitBoxCenter = unitBox.center();
    const QPointF areaCenter = usable.center();

    for (int i = 0; i < tops.size(); ++i) {
        DirSizeNode *node = tops[i];
        const QPointF finalCenter = areaCenter + (top.pos[i] - unitBoxCenter) * fitScale;
        const qreal finalRadius = unitRadii[i] * fitScale;

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
        if (!labelBandRect(g, m).isEmpty()) {
            discR = std::max<qreal>(0, discR - m.labelBand / 2.0);
            discCenter += QPointF(0, m.labelBand / 2.0);
        }
        if (discR <= 0)
            continue;

        QVector<qreal> kidRadii(kids.size());
        qreal kidMaxRadius = 0;
        for (int j = 0; j < kids.size(); ++j) {
            kidRadii[j] = std::sqrt(static_cast<qreal>(kids[j]->size));
            kidMaxRadius = std::max(kidMaxRadius, kidRadii[j]);
        }
        kidMaxRadius = std::max(kidMaxRadius, 1e-9);
        QVector<qreal> unitKidRadii(kids.size());
        for (int j = 0; j < kids.size(); ++j)
            unitKidRadii[j] = kidRadii[j] / kidMaxRadius;

        PackCache::UnitPack localKid;
        if (!cache)
            localKid = toUnitPack(pack(unitKidRadii));
        const PackCache::UnitPack &kid = cache ? cache->nestedPack(node, unitKidRadii) : localKid;
        const qreal kidFitScale = discR / std::max(kid.enclosingR, 1e-6);

        for (int j = 0; j < kids.size(); ++j) {
            const qreal r = unitKidRadii[j] * kidFitScale;
            if (r < m.minBubbleR)
                continue;
            const QPointF pos = discCenter + (kid.pos[j] - kid.enclosingCenter) * kidFitScale;
            res.bubbles.append({pos, r, kids[j], groupIndex});
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
