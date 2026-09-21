#include "treemap_view.h"

#include <QContextMenuEvent>
#include <QEasingCurve>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QResizeEvent>
#include <QToolTip>

#include <algorithm>

#include "dpi.h"
#include "utilities.h"

namespace {

QRectF lerpRect(const QRectF &a, const QRectF &b, qreal t)
{
    return QRectF(a.x() + (b.x() - a.x()) * t,
                  a.y() + (b.y() - a.y()) * t,
                  a.width() + (b.width() - a.width()) * t,
                  a.height() + (b.height() - a.height()) * t);
}

QTransform mapRect(const QRectF &from, const QRectF &to)
{
    QTransform tf;
    tf.translate(to.x(), to.y());
    tf.scale(to.width() / from.width(), to.height() / from.height());
    tf.translate(-from.x(), -from.y());
    return tf;
}

} // namespace

TreemapView::TreemapView(QWidget *parent)
    : DiskMapView(parent)
{
    mZoom = new QVariantAnimation(this);
    mZoom->setDuration(200);
    mZoom->setEasingCurve(QEasingCurve::OutCubic);
    mZoom->setStartValue(0.0);
    mZoom->setEndValue(1.0);
    connect(mZoom, &QVariantAnimation::valueChanged, this, [this](const QVariant &v) {
        mZoomT = v.toReal();
        update();
    });
    connect(mZoom, &QVariantAnimation::finished, this, [this] {
        mZoomT = 1.0;
        mFromPixmap = QPixmap();
        update();
    });
}

TreemapLayout::Metrics TreemapView::scaledMetrics() const
{
    TreemapLayout::Metrics m;
    m.frameGap  = Dpi::scale(6);  m.tileGap   = Dpi::scale(3);
    m.headerH   = Dpi::scale(18); m.minFrameW = Dpi::scale(90);
    m.minFrameH = Dpi::scale(60);
    return m;
}

void TreemapView::rebuildLayout()
{
    const qreal pad = Dpi::scale(3);
    mLayout = TreemapLayout::build(mFocus, QRectF(rect()).adjusted(pad, pad, -pad, -pad), scaledMetrics());

    if (!mPendingZoom)
        return;
    mPendingZoom = false;

    if (!mZoomIn) {
        mZoomRect = QRectF();
        for (const auto &f : mLayout.frames)
            if (f.node == mZoomTarget) mZoomRect = f.outer;
        for (const auto &t : mLayout.tiles)
            if (t.node == mZoomTarget) mZoomRect = t.rect;
    }

    if (mZoomRect.isValid()) {
        mZoom->stop();
        mZoom->start();
    }
}

void TreemapView::aboutToDrill(DirSizeNode *target, bool drillingIn)
{
    if (Utilities::prefersReducedMotion() || !isVisible())
        return;

    mZoomRect = QRectF();
    for (const auto &f : mLayout.frames)
        if (f.node == target) mZoomRect = f.outer;
    for (const auto &t : mLayout.tiles)
        if (t.node == target) mZoomRect = t.rect;

    mFromPixmap = QPixmap(size() * devicePixelRatioF());
    mFromPixmap.setDevicePixelRatio(devicePixelRatioF());
    mFromPixmap.fill(mBackgroundColor);
    {
        QPainter p(&mFromPixmap);
        p.setRenderHint(QPainter::Antialiasing);
        p.setRenderHint(QPainter::TextAntialiasing);
        paintLayout(p, mLayout, 1.0);
    }

    mZoomTarget = target;
    mZoomIn = drillingIn;
    mPendingZoom = drillingIn ? mZoomRect.isValid() : true;
}

void TreemapView::resizeEvent(QResizeEvent *event)
{
    mZoom->stop();
    mZoomT = 1.0;
    mFromPixmap = QPixmap();
    DiskMapView::resizeEvent(event);
}

void TreemapView::paintTile(QPainter &p, const QRectF &rectIn, DirSizeNode *node, bool hovered)
{
    QRectF r = rectIn;
    const qreal radius = std::min<qreal>(Dpi::scale(5), std::min(r.width(), r.height()) / 2);
    const bool tiny = std::min(r.width(), r.height()) < 4;

    QColor shadow = mBackgroundColor.darker(260);
    if (hovered) {
        r.translate(0, -Dpi::scale(2));
        if (!tiny) {
            for (int i = 4; i >= 1; --i) {
                shadow.setAlpha(28);
                p.setPen(Qt::NoPen); p.setBrush(shadow);
                p.drawRoundedRect(r.adjusted(-i, -i + 3, i, i + 3), radius + i, radius + i);
            }
        }
    } else if (!tiny) {
        shadow.setAlpha(110);
        p.setPen(Qt::NoPen); p.setBrush(shadow);
        p.drawRoundedRect(r.translated(0, 1), radius, radius);
    }

    const QColor base = colourFor(node);
    QLinearGradient g(r.topLeft(), r.bottomRight());
    g.setColorAt(0.0, base.lighter(128));
    g.setColorAt(0.45, base);
    g.setColorAt(1.0, base.darker(128));
    p.setPen(Qt::NoPen);
    p.setBrush(g);
    p.drawRoundedRect(r, radius, radius);

    if (!tiny && r.width() > 2 * radius) {
        QColor hi = base.lighter(170); hi.setAlpha(120);
        p.setPen(QPen(hi, 1));
        p.drawLine(QPointF(r.left() + radius, r.top() + 0.5), QPointF(r.right() - radius, r.top() + 0.5));
    }

    if (hovered) {
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(mTextColor, 1.5));
        p.drawRoundedRect(r, radius, radius);
    }

    const bool full = r.width() >= 60 && r.height() >= 22;
    if (full || (r.width() >= 40 && r.height() >= 14)) {
        p.setPen(mTextColor);
        QFont f = p.font(); f.setPointSizeF(full ? 10.0 : 9.0); p.setFont(f);
        const QRectF tr = r.adjusted(5, 3, -5, -3);
        const QString name = p.fontMetrics().elidedText(node->name, Qt::ElideRight, int(tr.width()));
        p.drawText(tr, Qt::AlignLeft | Qt::AlignTop,
                   full ? name + "\n" + formatBytes(node->size) : name);
    }
}

void TreemapView::paintLayout(QPainter &p, const TreemapLayout::Result &layout, qreal opacity)
{
    p.save();
    p.setOpacity(opacity);
    const qreal fr = Dpi::scale(8);
    for (const auto &f : layout.frames) {
        const QColor hue = colourFor(f.node);
        QColor shadow = mBackgroundColor.darker(260);
        for (int i = 3; i >= 1; --i) {
            shadow.setAlpha(24);
            p.setPen(Qt::NoPen); p.setBrush(shadow);
            p.drawRoundedRect(f.outer.adjusted(-i, -i + 3, i, i + 3), fr + i, fr + i);
        }
        QColor fill = hue;   fill.setAlpha(38);
        QColor edge = hue.lighter(130); edge.setAlpha(110);
        p.setBrush(mBackgroundColor); p.setPen(Qt::NoPen);
        p.drawRoundedRect(f.outer, fr, fr);
        p.setBrush(fill); p.setPen(QPen(edge, 1));
        p.drawRoundedRect(f.outer, fr, fr);

        QColor strip = hue; strip.setAlpha(80);
        QPainterPath clip; clip.addRoundedRect(f.outer, fr, fr);
        p.save(); p.setClipPath(clip); p.fillRect(f.header, strip); p.restore();

        p.setPen(mTextColor);
        QFont hf = p.font(); hf.setPointSizeF(9.5); hf.setBold(true); p.setFont(hf);
        const QRectF ht = f.header.adjusted(7, 0, -7, 0);
        const QString size = formatBytes(f.node->size);
        const int sizeW = p.fontMetrics().horizontalAdvance(size) + 8;
        p.drawText(ht, Qt::AlignVCenter | Qt::AlignLeft,
                   p.fontMetrics().elidedText(f.node->name, Qt::ElideRight, int(ht.width()) - sizeW));
        hf.setBold(false); p.setFont(hf);
        p.drawText(ht, Qt::AlignVCenter | Qt::AlignRight, size);
    }
    const TreemapLayout::Tile *hot = nullptr;
    for (const auto &t : layout.tiles) {
        if (t.node == hoveredNode()) { hot = &t; continue; }
        paintTile(p, t.rect, t.node, false);
    }
    if (hot)
        paintTile(p, hot->rect, hot->node, true);

    for (const auto &f : layout.frames) {
        if (f.node == hoveredNode()) {
            p.setBrush(Qt::NoBrush);
            p.setPen(QPen(mTextColor, 1.5));
            p.drawRoundedRect(f.outer, fr, fr);
            break;
        }
    }
    p.restore();
}

void TreemapView::paintEvent(QPaintEvent * /*event*/)
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
        return;
    }

    if (mLayout.tiles.isEmpty() && mLayout.frames.isEmpty()) {
        p.setPen(mTextColor);
        p.drawText(rect(), Qt::AlignCenter,
                   tr("This folder is empty."));
        return;
    }

    if (mZoom->state() == QAbstractAnimation::Running) {
        p.setRenderHint(QPainter::SmoothPixmapTransform);
        const QRectF full(rect());
        const qreal t = mZoomT;

        if (mZoomIn) {
            const QRectF cur = lerpRect(mZoomRect, full, t);
            p.save();
            p.setOpacity(1.0 - t);
            p.setTransform(mapRect(mZoomRect, cur), true);
            p.drawPixmap(rect(), mFromPixmap);
            p.restore();

            p.save();
            p.setTransform(mapRect(full, cur), true);
            paintLayout(p, mLayout, t);
            p.restore();
        } else {
            const QRectF cur = lerpRect(full, mZoomRect, t);
            p.save();
            p.setOpacity(1.0 - t);
            p.setTransform(mapRect(full, cur), true);
            p.drawPixmap(rect(), mFromPixmap);
            p.restore();

            p.save();
            p.setTransform(mapRect(mZoomRect, cur), true);
            paintLayout(p, mLayout, t);
            p.restore();
        }
        return;
    }

    paintLayout(p, mLayout, 1.0);
}

void TreemapView::mouseMoveEvent(QMouseEvent *event)
{
    if (mZoom->state() == QAbstractAnimation::Running)
        return;

    DirSizeNode *node = TreemapLayout::hitTest(mLayout, event->position());
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

void TreemapView::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (mZoom->state() == QAbstractAnimation::Running)
        return;

    DirSizeNode *node = TreemapLayout::hitTest(mLayout, event->position());
    requestDrillIfDir(node);
}

void TreemapView::contextMenuEvent(QContextMenuEvent *event)
{
    if (mZoom->state() == QAbstractAnimation::Running)
        return;

    DirSizeNode *node = TreemapLayout::hitTest(mLayout, event->pos());
    showContextMenuFor(node, event->globalPos());
}

void TreemapView::leaveEvent(QEvent * /*event*/)
{
    setHoveredNode(nullptr);
    QToolTip::hideText();
}
