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

    // Ring 0 keeps every size>0 child regardless of minSweepDeg — mirrors
    // the single-ring sunburst's existing behaviour, which never dropped
    // small wedges either.
    qreal cursor = 0;
    res.wedges.reserve(children.size());
    for (auto *n : children) {
        Wedge w;
        w.startDeg = cursor;
        w.sweepDeg = 360.0 * (static_cast<qreal>(n->size) / total);
        w.innerR = res.ring0InnerR;
        w.outerR = res.ring0OuterR;
        w.node = n;
        w.ring = 0;
        res.wedges.append(w);
        cursor += w.sweepDeg;
    }

    const int ring0Count = res.wedges.size();
    for (int i = 0; i < ring0Count; ++i) {
        const Wedge parent = res.wedges[i]; // copy: res.wedges grows below
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
        int addedBefore = res.wedges.size();
        for (auto *g : grandkids) {
            const qreal sweep = parent.sweepDeg * (static_cast<qreal>(g->size) / grandTotal);
            if (sweep >= m.minSweepDeg) {
                Wedge w;
                w.startDeg = gcursor;
                w.sweepDeg = sweep;
                w.innerR = res.ring1InnerR;
                w.outerR = res.ring1OuterR;
                w.node = g;
                w.ring = 1;
                w.parentIndex = i;
                res.wedges.append(w);
            }
            gcursor += sweep;
        }

        // Every grandchild was too small to survive minSweepDeg — fall back
        // to a single placeholder so the parent's arc isn't left blank.
        if (res.wedges.size() == addedBefore) {
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
