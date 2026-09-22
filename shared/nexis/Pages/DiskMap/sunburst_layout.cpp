#include "sunburst_layout.h"

#include <algorithm>
#include <cmath>

namespace {

// Degrees clockwise from 12 o'clock, in [0, 360) — atan2(x, -y) puts
// straight up (screen -y) at 0° and straight right (screen +x) at 90°.
qreal chartAngleDeg(const QPointF &v)
{
    qreal deg = std::atan2(v.x(), -v.y()) * 180.0 / M_PI;
    if (deg < 0)
        deg += 360.0;
    return deg;
}

QVector<DirSizeNode*> sortedSizedChildren(DirSizeNode *parent)
{
    QVector<DirSizeNode*> kids;
    for (auto &c : parent->children)
        if (c->size > 0)
            kids.append(c.get());
    std::sort(kids.begin(), kids.end(),
              [](DirSizeNode *a, DirSizeNode *b) { return a->size > b->size; });
    return kids;
}

// A point's angle can land a hair outside [start, start+sweep] from
// float drift accumulated across many `cursor += sweep` additions, and a
// wedge that legitimately straddles the 0°/360° seam needs the wrapped
// comparison too.
bool angleWithin(qreal deg, qreal startDeg, qreal sweepDeg)
{
    constexpr qreal eps = 1e-4;
    const qreal end = startDeg + sweepDeg;
    if (deg >= startDeg - eps && deg <= end + eps)
        return true;
    if (deg + 360.0 >= startDeg - eps && deg + 360.0 <= end + eps)
        return true;
    if (deg - 360.0 >= startDeg - eps && deg - 360.0 <= end + eps)
        return true;
    return false;
}

// A wedge is drawn individually only if it clears both an absolute degree
// floor and a radius-aware minimum arc length — a fan of many same-degree
// slivers reads very differently near the hub than out at the rim, so a
// fixed-degree cutoff alone either over- or under-aggregates depending on
// which ring it's applied to.
bool arcKept(qreal sweepDeg, qreal midR, const SunburstLayout::Metrics &m)
{
    if (sweepDeg < m.minSweepDeg)
        return false;
    const qreal arcLen = midR * (sweepDeg * M_PI / 180.0);
    return arcLen >= m.minArcPx;
}

} // namespace

SunburstLayout::Result SunburstLayout::build(DirSizeNode *focus, const QRectF &area, const Metrics &m)
{
    Result res;
    if (!focus || !focus->isDir || focus->size <= 0)
        return res;

    const QRectF usable = area.adjusted(m.margin, m.margin, -m.margin, -m.margin);
    if (usable.width() <= 0 || usable.height() <= 0)
        return res;

    res.outerR = std::min(usable.width(), usable.height()) / 2.0;
    res.center = usable.center();
    res.hubR = res.outerR * m.hubRatio;
    res.ring0InnerR = res.hubR + m.ringGap;
    res.ring0OuterR = res.outerR * m.innerRingRatio;
    res.ring1InnerR = res.ring0OuterR + m.ringGap;
    res.ring1OuterR = res.outerR;

    if (res.ring0OuterR <= res.ring0InnerR || res.ring1OuterR <= res.ring1InnerR)
        return res;

    QVector<DirSizeNode*> children = sortedSizedChildren(focus);
    if (children.isEmpty())
        return res;
    qreal total = 0;
    for (auto *c : children)
        total += c->size;
    if (total <= 0)
        return res;

    // Ring 0: children are size-descending, so at a fixed mid-radius arc
    // length is monotonic in size — once one child's wedge is too thin to
    // draw, every child after it is too. Stop there and fold the rest into
    // one trailing remainder wedge owned by `focus` itself, rather than
    // leaving their span blank.
    const qreal ring0MidR = (res.ring0InnerR + res.ring0OuterR) / 2.0;
    qreal cursor = 0;
    res.wedges.reserve(children.size());
    int i0 = 0;
    for (; i0 < children.size(); ++i0) {
        DirSizeNode *n = children[i0];
        const qreal sweep = 360.0 * (static_cast<qreal>(n->size) / total);
        // Always show at least minKeptWedges siblings (largest first) even
        // if their arc is below minArcPx, so a folder full of many
        // similar-sized children still renders navigable real wedges rather
        // than collapsing entirely into one blank remainder — as long as
        // the wedge still clears the absolute degree floor.
        const bool keptByFloor = i0 < m.minKeptWedges && sweep >= m.minSweepDeg;
        if (!arcKept(sweep, ring0MidR, m) && !keptByFloor)
            break;
        Wedge w;
        w.startDeg = cursor;
        w.sweepDeg = sweep;
        w.innerR = res.ring0InnerR;
        w.outerR = res.ring0OuterR;
        w.node = n;
        w.ring = 0;
        res.wedges.append(w);
        cursor += sweep;
    }
    if (i0 < children.size()) {
        const qreal remSweep = 360.0 - cursor;
        if (remSweep > 1e-6) {
            Wedge rem;
            rem.startDeg = cursor;
            rem.sweepDeg = remSweep;
            rem.innerR = res.ring0InnerR;
            rem.outerR = res.ring0OuterR;
            rem.node = focus;
            rem.ring = 0;
            rem.remainder = true;
            res.wedges.append(rem);
        }
    }

    const int ring0Count = res.wedges.size();
    const qreal ring1MidR = (res.ring1InnerR + res.ring1OuterR) / 2.0;
    for (int i = 0; i < ring0Count; ++i) {
        const Wedge parent = res.wedges[i]; // copy: res.wedges grows below

        if (parent.remainder) {
            // Aggregated top-level items: no single real directory to
            // subdivide, but the ring-1 band under it should still read
            // as "more of the same" rather than a gap.
            Wedge ph;
            ph.startDeg = parent.startDeg;
            ph.sweepDeg = parent.sweepDeg;
            ph.innerR = res.ring1InnerR;
            ph.outerR = res.ring1OuterR;
            ph.node = focus;
            ph.ring = 1;
            ph.parentIndex = i;
            ph.placeholder = true;
            res.wedges.append(ph);
            continue;
        }

        DirSizeNode *pnode = parent.node;
        QVector<DirSizeNode*> grandkids = pnode->isDir ? sortedSizedChildren(pnode) : QVector<DirSizeNode*>();
        if (grandkids.isEmpty()) {
            Wedge ph;
            ph.startDeg = parent.startDeg;
            ph.sweepDeg = parent.sweepDeg;
            ph.innerR = res.ring1InnerR;
            ph.outerR = res.ring1OuterR;
            ph.node = pnode;
            ph.ring = 1;
            ph.parentIndex = i;
            ph.placeholder = true;
            res.wedges.append(ph);
            continue;
        }

        qreal grandTotal = 0;
        for (auto *g : grandkids)
            grandTotal += g->size;

        qreal gcursor = parent.startDeg;
        int j = 0;
        for (; j < grandkids.size(); ++j) {
            const qreal sweep = parent.sweepDeg * (static_cast<qreal>(grandkids[j]->size) / grandTotal);
            // Same minKeptWedges floor as ring 0 — see the comment there.
            const bool keptByFloor = j < m.minKeptWedges && sweep >= m.minSweepDeg;
            if (!arcKept(sweep, ring1MidR, m) && !keptByFloor)
                break;
            Wedge w;
            w.startDeg = gcursor;
            w.sweepDeg = sweep;
            w.innerR = res.ring1InnerR;
            w.outerR = res.ring1OuterR;
            w.node = grandkids[j];
            w.ring = 1;
            w.parentIndex = i;
            res.wedges.append(w);
            gcursor += sweep;
        }

        // Remaining grandkids (possibly all of them) were too thin to draw
        // individually — one trailing remainder wedge, owned by the parent
        // directory itself, covers the rest of its arc.
        if (j < grandkids.size()) {
            const qreal remSweep = (parent.startDeg + parent.sweepDeg) - gcursor;
            if (remSweep > 1e-6) {
                Wedge rem;
                rem.startDeg = gcursor;
                rem.sweepDeg = remSweep;
                rem.innerR = res.ring1InnerR;
                rem.outerR = res.ring1OuterR;
                rem.node = pnode;
                rem.ring = 1;
                rem.parentIndex = i;
                rem.remainder = true;
                res.wedges.append(rem);
            }
        }
    }

    return res;
}

DirSizeNode *SunburstLayout::hitTest(const Result &r, const QPointF &pos)
{
    if (r.wedges.isEmpty())
        return nullptr;

    const QPointF v = pos - r.center;
    const qreal dist = std::hypot(v.x(), v.y());
    if (dist < r.hubR || dist > r.outerR)
        return nullptr;

    const qreal deg = chartAngleDeg(v);
    for (const Wedge &w : r.wedges) {
        if (dist < w.innerR || dist > w.outerR)
            continue;
        if (angleWithin(deg, w.startDeg, w.sweepDeg))
            return w.node;
    }
    return nullptr;
}
