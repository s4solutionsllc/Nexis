#include "bubble_map_view.h"

#include <QContextMenuEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QToolTip>

#include <algorithm>
#include <cmath>

BubbleMapView::BubbleMapView(QWidget *parent)
    : DiskMapView(parent)
{
}

// Packs `items` (already sorted, sizes > 0) into non-overlapping circles
// whose areas are proportional to size. Circles are seeded on a
// golden-angle spiral, then relaxed for a fixed number of iterations with a
// pairwise separation force plus a gentle centroid pull — approximate but
// visually tight, and O(n^2 * iterations) is trivial for the sibling counts
// a disk map actually shows.
void BubbleMapView::packCircles(const QVector<DirSizeNode*> &items)
{
    const int n = items.size();
    if (n == 0)
        return;

    QVector<qreal> r(n);
    for (int i = 0; i < n; ++i)
        r[i] = std::sqrt(static_cast<qreal>(items[i]->size));

    QVector<QPointF> pos(n);
    const qreal golden = 137.50776405 * M_PI / 180.0;
    qreal spread = 0;
    for (int i = 0; i < n; ++i)
        spread += r[i];
    spread = std::max(spread, 1.0);
    for (int i = 0; i < n; ++i) {
        const qreal a = i * golden;
        const qreal radius = spread * std::sqrt(static_cast<qreal>(i + 1) / n);
        pos[i] = QPointF(radius * std::cos(a), radius * std::sin(a));
    }

    // This is a visual disk map, not a physics sim: an approximate pack is
    // fine, so we run a fixed, small iteration count rather than iterating
    // to convergence.
    const int iterations = 200;
    for (int iter = 0; iter < iterations; ++iter) {
        for (int i = 0; i < n; ++i)
            pos[i] -= pos[i] * 0.02;

        for (int i = 0; i < n; ++i) {
            for (int j = i + 1; j < n; ++j) {
                QPointF d = pos[j] - pos[i];
                const qreal dist = std::hypot(d.x(), d.y());
                const qreal minDist = r[i] + r[j];
                if (dist < minDist) {
                    const qreal overlap = minDist - dist;
                    const QPointF dir = dist > 1e-6 ? d / dist : QPointF(1, 0);
                    pos[i] -= dir * (overlap * 0.5);
                    pos[j] += dir * (overlap * 0.5);
                }
            }
        }
    }

    qreal boundingR = 1.0;
    for (int i = 0; i < n; ++i)
        boundingR = std::max(boundingR, std::hypot(pos[i].x(), pos[i].y()) + r[i]);

    const QRectF area = rect().adjusted(4, 4, -4, -4);
    if (area.width() <= 0 || area.height() <= 0)
        return;
    const qreal targetR = std::min(area.width(), area.height()) / 2.0;
    const qreal scale = targetR / boundingR;
    const QPointF center = area.center();

    mCircles.reserve(n);
    for (int i = 0; i < n; ++i) {
        Circle c;
        c.center = center + pos[i] * scale;
        c.radius = r[i] * scale;
        c.node = items[i];
        mCircles.append(c);
    }
}

void BubbleMapView::rebuildLayout()
{
    mCircles.clear();
    mHoveredCircle = nullptr;
    if (!mFocus || !mFocus->isDir || mFocus->size <= 0)
        return;

    QVector<DirSizeNode*> children;
    children.reserve(static_cast<int>(mFocus->children.size()));
    for (auto &c : mFocus->children) {
        if (c->size > 0)
            children.append(c.get());
    }
    if (children.isEmpty())
        return;
    std::sort(children.begin(), children.end(),
              [](DirSizeNode *a, DirSizeNode *b) { return a->size > b->size; });

    packCircles(children);
}

BubbleMapView::Circle *BubbleMapView::circleAt(const QPointF &pos)
{
    // Reverse iteration doesn't matter for correctness here (circles never
    // overlap once packed) but keeps hit-testing consistent with the other
    // modes' "last wins" convention.
    for (int i = mCircles.size() - 1; i >= 0; --i) {
        const QPointF d = pos - mCircles[i].center;
        if (std::hypot(d.x(), d.y()) <= mCircles[i].radius)
            return &mCircles[i];
    }
    return nullptr;
}

void BubbleMapView::paintEvent(QPaintEvent * /*event*/)
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

    if (mCircles.isEmpty()) {
        p.setPen(mTextColor);
        p.drawText(rect(), Qt::AlignCenter,
                   tr("This folder is empty."));
        return;
    }

    for (const Circle &c : mCircles) {
        p.setBrush(colourFor(c.node));
        p.setPen(QPen(mBorderColor, 1));
        p.drawEllipse(c.center, c.radius, c.radius);

        if (c.radius >= 28) {
            const QString label = c.node->name + "\n" + formatBytes(c.node->size);
            const QRectF textRect(c.center.x() - c.radius, c.center.y() - c.radius,
                                  c.radius * 2, c.radius * 2);
            p.setPen(mTextColor);
            QFont f = p.font();
            f.setPointSizeF(std::max(8.0, std::min(11.0, c.radius / 5.0)));
            p.setFont(f);
            p.drawText(textRect, Qt::AlignCenter | Qt::TextWordWrap, label);
        }
    }

    if (mHoveredCircle) {
        QPen pen(mTextColor, 2);
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(mHoveredCircle->center,
                      mHoveredCircle->radius - 1, mHoveredCircle->radius - 1);
    }
}

void BubbleMapView::mouseMoveEvent(QMouseEvent *event)
{
    Circle *c = circleAt(event->position());
    if (c != mHoveredCircle) {
        mHoveredCircle = c;
        setHoveredNode(c ? c->node : nullptr);
    }
    if (c) {
        QToolTip::showText(event->globalPosition().toPoint(),
                           QString("%1\n%2")
                               .arg(c->node->path)
                               .arg(formatBytes(c->node->size)),
                           this);
    } else {
        QToolTip::hideText();
    }
}

void BubbleMapView::mouseDoubleClickEvent(QMouseEvent *event)
{
    Circle *c = circleAt(event->position());
    requestDrillIfDir(c ? c->node : nullptr);
}

void BubbleMapView::contextMenuEvent(QContextMenuEvent *event)
{
    Circle *c = circleAt(event->pos());
    showContextMenuFor(c ? c->node : nullptr, event->globalPos());
}

void BubbleMapView::leaveEvent(QEvent * /*event*/)
{
    if (mHoveredCircle) {
        mHoveredCircle = nullptr;
        setHoveredNode(nullptr);
    }
    QToolTip::hideText();
}
