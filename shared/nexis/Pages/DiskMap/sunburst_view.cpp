#include "sunburst_view.h"

#include <QContextMenuEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QRadialGradient>
#include <QToolTip>

#include <algorithm>
#include <cmath>

#include "dpi.h"

namespace {

// Alpha-composite `fg` (its own alpha honoured) over the opaque `bg` —
// mirrors BubbleMapView's alphaOver(), used to work out a readable label
// colour against what's actually painted underneath, not a bare fill colour.
QColor alphaOver(const QColor &fg, const QColor &bg)
{
    const qreal a = fg.alphaF();
    return QColor::fromRgbF(fg.redF() * a + bg.redF() * (1.0 - a),
                            fg.greenF() * a + bg.greenF() * (1.0 - a),
                            fg.blueF() * a + bg.blueF() * (1.0 - a));
}

} // namespace

SunburstView::SunburstView(QWidget *parent)
    : DiskMapView(parent)
{
}

SunburstLayout::Metrics SunburstView::scaledMetrics() const
{
    SunburstLayout::Metrics m;
    m.ringGap    = Dpi::scale(3);
    m.wedgeGapPx = 1.5 * Dpi::factor();
    m.minArcPx   = Dpi::scale(6);
    m.margin     = Dpi::scale(8);
    return m;
}

void SunburstView::rebuildLayout()
{
    mLayout = SunburstLayout::build(mFocus, QRectF(rect()), scaledMetrics());
    startCrossFadeIfArmed();
}

void SunburstView::aboutToDrill(DirSizeNode *target, bool drillingIn)
{
    Q_UNUSED(target);
    Q_UNUSED(drillingIn);
    armCrossFade();
}

// `radialOffset` pushes the wedge outward concentrically (both radii grow)
// rather than translating it: a translation only looks like "push out" for a
// narrow wedge — a wide one slides sideways into the hub and its neighbours.
QPainterPath SunburstView::wedgePath(const SunburstLayout::Wedge &w, qreal radialOffset) const
{
    QPainterPath path;
    if (w.sweepDeg <= 0 || w.outerR <= w.innerR)
        return path;

    const qreal innerR = w.innerR + radialOffset;
    const qreal outerR = w.outerR + radialOffset;
    const qreal midR = (innerR + outerR) / 2.0;
    qreal gapDeg = 0;
    if (midR > 1.0)
        gapDeg = (scaledMetrics().wedgeGapPx / midR) * (180.0 / M_PI);

    qreal start = w.startDeg;
    qreal sweep = w.sweepDeg;
    if (gapDeg * 2.0 < sweep) {
        start += gapDeg;
        sweep -= gapDeg * 2.0;
    }
    if (sweep <= 0)
        return path;

    const QRectF outerRect(mLayout.center.x() - outerR, mLayout.center.y() - outerR,
                           outerR * 2, outerR * 2);
    const QRectF innerRect(mLayout.center.x() - innerR, mLayout.center.y() - innerR,
                           innerR * 2, innerR * 2);

    // Qt angles are degrees CCW from 3 o'clock; our chart angles are degrees
    // CW from 12 o'clock, so qtAngle = 90 - chartAngle.
    const qreal qtStart = 90.0 - start;
    const qreal qtSweep = -sweep;
    path.arcMoveTo(outerRect, qtStart);
    path.arcTo(outerRect, qtStart, qtSweep);
    path.arcTo(innerRect, qtStart + qtSweep, -qtSweep);
    path.closeSubpath();
    return path;
}

void SunburstView::paintShadowDisc(QPainter &p)
{
    if (mLayout.outerR < 4)
        return;
    QColor shadow = mBackgroundColor.darker(260);
    // Outer rings fainter, innermost (closest to the disc's own rim)
    // strongest — a real falloff instead of three passes at one flat alpha.
    constexpr int kSteps = 3;
    for (int i = kSteps; i >= 1; --i) {
        const qreal t = static_cast<qreal>(kSteps - i) / (kSteps - 1);
        shadow.setAlpha(int(8 + t * 14));
        p.setPen(Qt::NoPen);
        p.setBrush(shadow);
        p.drawEllipse(mLayout.center + QPointF(0, i), mLayout.outerR + i * 0.5, mLayout.outerR + i * 0.5);
    }
}

void SunburstView::paintWedge(QPainter &p, const SunburstLayout::Wedge &w, bool hovered)
{
    if (w.sweepDeg <= 0 || w.outerR <= w.innerR)
        return;

    const qreal pushOffset = hovered ? Dpi::scale(5) : 0;
    const QPainterPath path = wedgePath(w, pushOffset);
    if (path.isEmpty())
        return;

    QColor fill;
    if (w.remainder && w.ring == 0) {
        // A ring-0 remainder's node is `focus` itself, which assignHues()
        // never assigns a slot to (it only walks focus's descendants) — so
        // colourFor() would silently fall back to slot 0, i.e. borrow the
        // largest real child's hue. Use a theme-neutral colour instead,
        // blended from the border/background rather than any hue.
        float bh, bs, bl, ba;
        mBackgroundColor.getHslF(&bh, &bs, &bl, &ba);
        float rh, rs, rl, ra;
        mBorderColor.getHslF(&rh, &rs, &rl, &ra);
        fill = mBorderColor;
        fill.setHslF(rh, rs * 0.5f, (rl + bl) / 2.0f, ra);
    } else {
        // Ring-1 remainders keep a muted version of the real parent
        // directory's hue (w.node is the parent, not `focus`) so they still
        // read as "more of the same folder" rather than a neutral gap.
        fill = colourFor(w.node);
    }
    if (w.placeholder || w.remainder) {
        float h, s, l, a;
        fill.getHslF(&h, &s, &l, &a);
        s *= 0.4f;
        l = std::max(0.0f, l - 0.04f);
        fill.setHslF(h, s, l, a);
    }

    p.setPen(Qt::NoPen);
    p.setBrush(fill);
    p.drawPath(path);

    // Per-ring "raised band" lighting: shade near the inner edge fading
    // through transparent at mid-band to a highlight at the outer edge,
    // derived purely from the theme background so it reads consistently
    // across every wedge's own hue.
    const qreal innerFrac = std::clamp((w.innerR + pushOffset) / (w.outerR + pushOffset), 0.0, 1.0);
    const qreal midFrac = std::clamp((innerFrac + 1.0) / 2.0, 0.0, 1.0);
    QColor shade = mBackgroundColor.darker(200);
    shade.setAlpha(55);
    QColor highlight = mBackgroundColor.lighter(200);
    highlight.setAlpha(45);
    QColor mid = shade;
    mid.setAlpha(0);

    QRadialGradient grad(mLayout.center, w.outerR + pushOffset);
    grad.setColorAt(innerFrac, shade);
    grad.setColorAt(midFrac, mid);
    grad.setColorAt(1.0, highlight);
    p.fillPath(path, grad);

    if (hovered) {
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(mTextColor, 1.5));
        p.drawPath(path);
    }

    if (w.placeholder || w.remainder)
        return;

    const bool ring0 = w.ring == 0;
    const bool labelEligible = ring0
        ? w.sweepDeg >= 8.0
        : (w.sweepDeg >= 14.0 && (w.outerR - w.innerR) >= 40.0);
    if (!labelEligible)
        return;

    const qreal midDeg = w.startDeg + w.sweepDeg / 2.0;
    const qreal midRad = midDeg * M_PI / 180.0;
    const qreal midR = (w.innerR + w.outerR) / 2.0;
    QPointF labelPos = mLayout.center + QPointF(midR * std::sin(midRad), -midR * std::cos(midRad));
    if (hovered)
        labelPos += QPointF(pushOffset * std::sin(midRad), -pushOffset * std::cos(midRad));

    const qreal sweepRad = std::min(w.sweepDeg, 180.0) * M_PI / 180.0;
    const qreal chord = 2.0 * midR * std::sin(sweepRad / 2.0);
    const int textW = int(chord) - 6;
    if (textW < 12)
        return;

    // The label sits at midR, i.e. exactly at the gradient's midFrac stop
    // (see its derivation above) — composite that stop's own colour over
    // fill, mirroring how bubble_map_view composites a membrane's fill over
    // the page background, instead of assuming the bare fill is what's
    // actually on screen there.
    const QColor onScreen = alphaOver(mid, fill);

    QFont f = p.font();
    f.setBold(true);
    f.setPointSizeF(9.5);
    p.setFont(f);
    p.setPen(labelColourOn(onScreen));
    const QString elided = p.fontMetrics().elidedText(w.node->name, Qt::ElideRight, textW);
    const QRectF labelRect(labelPos.x() - chord / 2.0, labelPos.y() - f.pointSizeF(),
                           chord, f.pointSizeF() * 2);
    p.drawText(labelRect, Qt::AlignCenter, elided);
}

void SunburstView::paintHub(QPainter &p)
{
    if (mLayout.hubR < 2 || !mFocus)
        return;

    QColor hubFill = mBackgroundColor;
    float h, s, l, a;
    hubFill.getHslF(&h, &s, &l, &a);
    hubFill.setHslF(h, s, l > 0.5f ? std::max(0.0f, l - 0.05f) : std::min(1.0f, l + 0.07f), a);

    p.setPen(Qt::NoPen);
    p.setBrush(hubFill);
    p.drawEllipse(mLayout.center, mLayout.hubR, mLayout.hubR);

    QColor rim = mBorderColor;
    rim.setAlpha(140);
    p.setPen(QPen(rim, 1));
    p.setBrush(Qt::NoBrush);
    p.drawEllipse(mLayout.center, mLayout.hubR, mLayout.hubR);

    p.setPen(labelColourOn(hubFill));
    QFont f = p.font();
    f.setBold(true);
    f.setPointSizeF(9.0);
    p.setFont(f);
    const QString centerLabel = mFocus->name.isEmpty() ? mFocus->path : mFocus->name;
    const QRectF hubRect(mLayout.center.x() - mLayout.hubR, mLayout.center.y() - mLayout.hubR,
                         mLayout.hubR * 2, mLayout.hubR * 2);
    p.drawText(hubRect, Qt::AlignCenter | Qt::TextWordWrap,
               centerLabel + "\n" + formatBytes(mFocus->size));
}

void SunburstView::paintEvent(QPaintEvent * /*event*/)
{
    if (!mBackgroundColor.isValid())
        return;

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);
    p.fillRect(rect(), mBackgroundColor);

    if (!mFocus) {
        p.setPen(mTextColor);
        p.drawText(rect(), Qt::AlignCenter,
                   tr("No scan loaded. Choose a folder and press Scan."));
    } else if (mLayout.wedges.isEmpty()) {
        p.setPen(mTextColor);
        p.drawText(rect(), Qt::AlignCenter,
                   tr("This folder is empty."));
    } else {
        paintShadowDisc(p);

        DirSizeNode *hn = hoveredNode();
        for (const auto &w : mLayout.wedges)
            if (w.node != hn)
                paintWedge(p, w, false);
        for (const auto &w : mLayout.wedges)
            if (w.node == hn)
                paintWedge(p, w, true);

        paintHub(p);
    }

    paintCrossFadeOverlay(p);
}

void SunburstView::mouseMoveEvent(QMouseEvent *event)
{
    DirSizeNode *node = SunburstLayout::hitTest(mLayout, event->position());
    setHoveredNode(node);
    if (node) {
        QToolTip::showText(event->globalPosition().toPoint(),
                           QString("%1\n%2")
                               .arg(node->path)
                               .arg(formatBytes(node->size)),
                           this);
    } else {
        QToolTip::hideText();
    }
}

void SunburstView::mouseDoubleClickEvent(QMouseEvent *event)
{
    DirSizeNode *node = SunburstLayout::hitTest(mLayout, event->position());
    requestDrillIfDir(node);
}

void SunburstView::contextMenuEvent(QContextMenuEvent *event)
{
    DirSizeNode *node = SunburstLayout::hitTest(mLayout, event->pos());
    showContextMenuFor(node, event->globalPos());
}

void SunburstView::leaveEvent(QEvent * /*event*/)
{
    setHoveredNode(nullptr);
    QToolTip::hideText();
}
