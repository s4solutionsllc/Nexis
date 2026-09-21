#include "bubble_map_view.h"

#include <QContextMenuEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QRadialGradient>
#include <QToolTip>

#include <algorithm>
#include <cmath>

#include "dpi.h"

namespace {

// Alpha-composite `fg` (its own alpha honoured) over the opaque `bg` —
// mirrors TreemapView's alphaOver(), used to work out a readable label
// colour against the translucent membrane fill actually painted underneath.
QColor alphaOver(const QColor &fg, const QColor &bg)
{
    const qreal a = fg.alphaF();
    return QColor::fromRgbF(fg.redF() * a + bg.redF() * (1.0 - a),
                            fg.greenF() * a + bg.greenF() * (1.0 - a),
                            fg.blueF() * a + bg.blueF() * (1.0 - a));
}

// Width of the horizontal chord of a circle of radius `r` at vertical
// distance `dy` from its center.
qreal chordAt(qreal r, qreal dy)
{
    return 2.0 * std::sqrt(std::max<qreal>(0, r * r - dy * dy));
}

} // namespace

BubbleMapView::BubbleMapView(QWidget *parent)
    : DiskMapView(parent)
{
}

BubbleLayout::Metrics BubbleMapView::scaledMetrics() const
{
    BubbleLayout::Metrics m;
    m.minGroupR   = Dpi::scale(45);
    m.innerPad    = Dpi::scale(4);
    m.labelBand   = Dpi::scale(16);
    m.minBubbleR  = 1.5;
    m.outerMargin = Dpi::scale(4);
    return m;
}

void BubbleMapView::rebuildLayout()
{
    mLayout = BubbleLayout::build(mFocus, QRectF(rect()), scaledMetrics(), &mPackCache);
    startCrossFadeIfArmed();
}

void BubbleMapView::aboutToDrill(DirSizeNode *target, bool drillingIn)
{
    Q_UNUSED(target);
    Q_UNUSED(drillingIn);
    armCrossFade();
}

void BubbleMapView::rootAboutToChange()
{
    // Cached nested/top-level packs are keyed by DirSizeNode* — pointers
    // from the tree being replaced must never be looked up again.
    mPackCache.clear();
}

void BubbleMapView::paintGroup(QPainter &p, const BubbleLayout::Group &g, bool hovered)
{
    if (g.radius < 4)
        return;

    const QColor hue = colourFor(g.node);

    QColor shadow = mBackgroundColor.darker(260);
    for (int i = 3; i >= 1; --i) {
        shadow.setAlpha(22);
        p.setPen(Qt::NoPen);
        p.setBrush(shadow);
        p.drawEllipse(g.center + QPointF(0, i), g.radius + i * 0.5, g.radius + i * 0.5);
    }

    // Fill alpha raised to 60-70 (was 40): at 40 the tint was too weak to
    // read against a light theme's near-white background, so the membrane
    // looked like flat grey rather than a tint of the folder's hue.
    QColor fill = hue; fill.setAlpha(65);
    QColor rim = hue.lighter(hovered ? 160 : 130);
    rim.setAlpha(hovered ? 210 : 115);

    p.setBrush(fill);
    p.setPen(QPen(rim, hovered ? Dpi::scale(2) : 1.2));
    p.drawEllipse(g.center, g.radius, g.radius);

    const QRectF labelRect = BubbleLayout::labelBandRect(g, scaledMetrics());
    if (!labelRect.isEmpty()) {
        // Composite the translucent fill over the actual page background
        // before picking a label colour against it — contrasting against
        // the un-composited (partly transparent) fill alone reads wrong.
        const QColor membrane = alphaOver(fill, mBackgroundColor);
        p.setPen(labelColourOn(membrane));
        QFont f = p.font();
        f.setPointSizeF(10.5);
        f.setBold(true);
        p.setFont(f);
        const QString text = g.node->name + "  ·  " + formatBytes(g.node->size);
        const int maxW = std::max(0, int(labelRect.width()) - 12);
        const QString elided = p.fontMetrics().elidedText(text, Qt::ElideRight, maxW);
        p.drawText(labelRect, Qt::AlignCenter, elided);
    }
}

void BubbleMapView::paintBubble(QPainter &p, const BubbleLayout::Bubble &b, bool hovered)
{
    QPointF c = b.center;
    const qreal r = b.radius;
    if (r <= 0)
        return;
    if (hovered)
        c -= QPointF(0, Dpi::scale(2));

    const bool tiny = r < 4;
    const QColor base = colourFor(b.node);

    if (!tiny) {
        QColor shadow = mBackgroundColor.darker(260);
        if (hovered) {
            for (int i = 4; i >= 1; --i) {
                shadow.setAlpha(26);
                p.setPen(Qt::NoPen);
                p.setBrush(shadow);
                p.drawEllipse(c + QPointF(0, i + 3), r + i, r + i);
            }
        } else {
            shadow.setAlpha(100);
            p.setPen(Qt::NoPen);
            p.setBrush(shadow);
            p.drawEllipse(c + QPointF(0, 1), r, r);
        }
    }

    p.setPen(Qt::NoPen);
    QColor mid = base;
    if (tiny) {
        p.setBrush(base);
    } else {
        const QPointF focal = c + QPointF(-r * (0.5 - 0.32) * 2.0, -r * (0.5 - 0.26) * 2.0);
        QRadialGradient grad(c, r, focal);
        grad.setColorAt(0.0, base.lighter(150));
        grad.setColorAt(0.45, base);
        grad.setColorAt(1.0, base.darker(150));
        p.setBrush(grad);
    }
    p.drawEllipse(c, r, r);

    if (hovered) {
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(mTextColor, 1.5));
        p.drawEllipse(c, r, r);
    }

    if (r >= 24) {
        const bool full = r >= 34;
        QFont f = p.font();
        f.setPointSizeF(std::clamp(r / 5.0, 8.0, 11.0));
        f.setBold(true);
        p.setFont(f);
        p.setPen(labelColourOn(mid));

        const qreal nameDy = full ? 2.0 : -3.0;
        const int nameW = std::max(0, int(chordAt(r, nameDy)) - 6);
        const QString name = p.fontMetrics().elidedText(b.node->name, Qt::ElideRight, nameW);
        p.drawText(QRectF(c.x() - r, c.y() - nameDy - f.pointSizeF(), r * 2, f.pointSizeF() * 2),
                   Qt::AlignHCenter | Qt::AlignVCenter, name);

        if (full) {
            QFont sf = f; sf.setBold(false); sf.setPointSizeF(std::max(8.0, f.pointSizeF() - 1.0));
            p.setFont(sf);
            const qreal sizeDy = -11.0;
            const int sizeW = std::max(0, int(chordAt(r, sizeDy)) - 6);
            const QString size = p.fontMetrics().elidedText(formatBytes(b.node->size), Qt::ElideRight, sizeW);
            p.drawText(QRectF(c.x() - r, c.y() - sizeDy - sf.pointSizeF(), r * 2, sf.pointSizeF() * 2),
                       Qt::AlignHCenter | Qt::AlignVCenter, size);
        }
    }
}

void BubbleMapView::paintLayout(QPainter &p, const BubbleLayout::Result &layout)
{
    DirSizeNode *hn = hoveredNode();

    for (const auto &g : layout.groups)
        paintGroup(p, g, g.node == hn);

    const BubbleLayout::Bubble *hot = nullptr;
    for (const auto &b : layout.bubbles) {
        if (b.node == hn) { hot = &b; continue; }
        paintBubble(p, b, false);
    }
    if (hot)
        paintBubble(p, *hot, true);
}

void BubbleMapView::paintEvent(QPaintEvent * /*event*/)
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
    } else if (mLayout.groups.isEmpty() && mLayout.bubbles.isEmpty()) {
        p.setPen(mTextColor);
        p.drawText(rect(), Qt::AlignCenter,
                   tr("This folder is empty."));
    } else {
        paintLayout(p, mLayout);
    }

    paintCrossFadeOverlay(p);
}

void BubbleMapView::mouseMoveEvent(QMouseEvent *event)
{
    DirSizeNode *node = BubbleLayout::hitTest(mLayout, event->position());
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

void BubbleMapView::mouseDoubleClickEvent(QMouseEvent *event)
{
    DirSizeNode *node = BubbleLayout::hitTest(mLayout, event->position());
    requestDrillIfDir(node);
}

void BubbleMapView::contextMenuEvent(QContextMenuEvent *event)
{
    DirSizeNode *node = BubbleLayout::hitTest(mLayout, event->pos());
    showContextMenuFor(node, event->globalPos());
}

void BubbleMapView::leaveEvent(QEvent * /*event*/)
{
    setHoveredNode(nullptr);
    QToolTip::hideText();
}
