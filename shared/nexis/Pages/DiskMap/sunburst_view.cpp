#include "sunburst_view.h"

#include <QContextMenuEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QToolTip>

#include <algorithm>
#include <cmath>

namespace {

// Degrees clockwise from 12 o'clock, in [0, 360) — the angle convention
// used for both wedge storage and hit-testing. atan2(x, -y) puts straight
// up (screen -y) at 0° and straight right (screen +x) at 90°, i.e. clockwise
// from the top, matching how the wedges are laid out below.
qreal chartAngleDeg(const QPointF &v)
{
    qreal deg = std::atan2(v.x(), -v.y()) * 180.0 / M_PI;
    if (deg < 0)
        deg += 360.0;
    return deg;
}

} // namespace

SunburstView::SunburstView(QWidget *parent)
    : DiskMapView(parent)
{
}

void SunburstView::rebuildLayout()
{
    mWedges.clear();
    mHoveredWedge = nullptr;
    mOuterR = mInnerR = 0;
    if (!mFocus || !mFocus->isDir || mFocus->size <= 0)
        return;

    QVector<DirSizeNode*> children;
    children.reserve(static_cast<int>(mFocus->children.size()));
    qreal total = 0;
    for (auto &c : mFocus->children) {
        if (c->size > 0) {
            children.append(c.get());
            total += c->size;
        }
    }
    if (children.isEmpty() || total <= 0)
        return;
    std::sort(children.begin(), children.end(),
              [](DirSizeNode *a, DirSizeNode *b) { return a->size > b->size; });

    const QRectF area = rect().adjusted(4, 4, -4, -4);
    if (area.width() <= 0 || area.height() <= 0)
        return;
    mOuterR = std::min(area.width(), area.height()) / 2.0;
    mInnerR = mOuterR * 0.35;
    mCenter = area.center();

    qreal cursor = 0;
    mWedges.reserve(children.size());
    for (auto *n : children) {
        Wedge w;
        w.startDeg = cursor;
        w.sweepDeg = 360.0 * (static_cast<qreal>(n->size) / total);
        w.node = n;
        mWedges.append(w);
        cursor += w.sweepDeg;
    }
}

SunburstView::Wedge *SunburstView::wedgeAt(const QPointF &pos)
{
    if (mWedges.isEmpty())
        return nullptr;

    const QPointF v = pos - mCenter;
    const qreal dist = std::hypot(v.x(), v.y());
    if (dist < mInnerR || dist > mOuterR)
        return nullptr;

    const qreal deg = chartAngleDeg(v);
    for (int i = 0; i < mWedges.size(); ++i) {
        Wedge &w = mWedges[i];
        if (deg >= w.startDeg && deg < w.startDeg + w.sweepDeg)
            return &w;
    }
    // Floating-point drift at the 360°/0° seam — the last wedge's upper
    // bound may land a hair short of the accumulated total.
    return &mWedges.last();
}

void SunburstView::paintEvent(QPaintEvent * /*event*/)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.fillRect(rect(), mBackgroundColor);

    if (!mFocus) {
        p.setPen(mTextColor);
        p.drawText(rect(), Qt::AlignCenter,
                   tr("No scan loaded. Choose a folder and press Scan."));
        return;
    }

    if (mWedges.isEmpty()) {
        p.setPen(mTextColor);
        p.drawText(rect(), Qt::AlignCenter,
                   tr("This folder is empty."));
        return;
    }

    const QRectF outerRect(mCenter.x() - mOuterR, mCenter.y() - mOuterR,
                           mOuterR * 2, mOuterR * 2);
    const QRectF innerRect(mCenter.x() - mInnerR, mCenter.y() - mInnerR,
                           mInnerR * 2, mInnerR * 2);

    auto donutPath = [&](const Wedge &w) {
        // Qt angles are degrees CCW from 3 o'clock; our chart angles are
        // degrees CW from 12 o'clock, so qtAngle = 90 - chartAngle.
        const qreal qtStart = 90.0 - w.startDeg;
        const qreal qtSweep = -w.sweepDeg;
        QPainterPath path;
        path.arcMoveTo(outerRect, qtStart);
        path.arcTo(outerRect, qtStart, qtSweep);
        path.arcTo(innerRect, qtStart + qtSweep, -qtSweep);
        path.closeSubpath();
        return path;
    };

    for (const Wedge &w : mWedges) {
        p.setBrush(colourFor(w.node));
        p.setPen(QPen(mBorderColor, 1));
        p.drawPath(donutPath(w));

        if (w.sweepDeg >= 8.0) {
            const qreal midDeg = w.startDeg + w.sweepDeg / 2.0;
            const qreal midR = (mInnerR + mOuterR) / 2.0;
            const qreal qtMidRad = (90.0 - midDeg) * M_PI / 180.0;
            const QPointF labelPos = mCenter + QPointF(midR * std::cos(qtMidRad),
                                                        -midR * std::sin(qtMidRad));
            p.setPen(mTextColor);
            QFont f = p.font();
            f.setPointSizeF(8.0);
            p.setFont(f);
            const QRectF labelRect(labelPos.x() - 40, labelPos.y() - 14, 80, 28);
            p.drawText(labelRect, Qt::AlignCenter | Qt::TextWordWrap, w.node->name);
        }
    }

    if (mHoveredWedge) {
        QPen pen(mTextColor, 2);
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);
        p.drawPath(donutPath(*mHoveredWedge));
    }

    // Centre hole: focus name/size, doubling as a visual "you are here".
    p.setPen(mTextColor);
    QFont f = p.font();
    f.setPointSizeF(9.0);
    p.setFont(f);
    const QString centerLabel = mFocus->name.isEmpty() ? mFocus->path : mFocus->name;
    p.drawText(innerRect, Qt::AlignCenter | Qt::TextWordWrap,
               centerLabel + "\n" + formatBytes(mFocus->size));
}

void SunburstView::mouseMoveEvent(QMouseEvent *event)
{
    Wedge *w = wedgeAt(event->position());
    if (w != mHoveredWedge) {
        mHoveredWedge = w;
        setHoveredNode(w ? w->node : nullptr);
    }
    if (w) {
        QToolTip::showText(event->globalPosition().toPoint(),
                           QString("%1\n%2")
                               .arg(w->node->path)
                               .arg(formatBytes(w->node->size)),
                           this);
    } else {
        QToolTip::hideText();
    }
}

void SunburstView::mouseDoubleClickEvent(QMouseEvent *event)
{
    Wedge *w = wedgeAt(event->position());
    requestDrillIfDir(w ? w->node : nullptr);
}

void SunburstView::contextMenuEvent(QContextMenuEvent *event)
{
    Wedge *w = wedgeAt(event->pos());
    showContextMenuFor(w ? w->node : nullptr, event->globalPos());
}

void SunburstView::leaveEvent(QEvent * /*event*/)
{
    if (mHoveredWedge) {
        mHoveredWedge = nullptr;
        setHoveredNode(nullptr);
    }
    QToolTip::hideText();
}
