#include "treemap_view.h"

#include <QAction>
#include <QContextMenuEvent>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QToolTip>

#include <algorithm>
#include <limits>

namespace {

// Format bytes the same way Format util would, but kept local so the test
// build can compile this in isolation (FormatUtil is in nexis-core which
// is fine; we just don't want a circular include from a test).
QString formatBytes(qint64 b)
{
    if (b < 1024)
        return QString::number(b) + " B";
    static const char *suffixes[] = {"KiB", "MiB", "GiB", "TiB", "PiB"};
    double v = static_cast<double>(b);
    int i = -1;
    do { v /= 1024.0; ++i; } while (v >= 1024.0 && i < 4);
    return QString::number(v, 'f', v < 10 ? 2 : 1) + " " + suffixes[i];
}

// Stable colour pick — keeps the same tile colour across re-layouts so the
// eye can follow a "hot" directory while drilling.
QColor stableColor(const QString &key, qreal saturation, qreal value)
{
    // Mix the name into a hue; simple FNV-1a so we don't drag in QHash::hash
    // determinism guarantees.
    quint32 h = 2166136261u;
    for (QChar c : key)
        h = (h ^ c.unicode()) * 16777619u;
    qreal hue = static_cast<qreal>(h % 360);
    QColor c;
    c.setHsvF(hue / 360.0, saturation, value);
    return c;
}

qreal worstAspect(const QVector<DirSizeNode*> &row, qint64 sum, qreal width)
{
    if (row.isEmpty() || sum <= 0 || width <= 0)
        return std::numeric_limits<qreal>::infinity();

    qint64 maxVal = 0, minVal = std::numeric_limits<qint64>::max();
    for (auto *n : row) {
        maxVal = std::max(maxVal, n->size);
        minVal = std::min(minVal, n->size);
    }
    // Treat zero-byte items as 1 so the divisor doesn't explode.
    if (minVal <= 0) minVal = 1;

    const qreal s2 = static_cast<qreal>(sum) * static_cast<qreal>(sum);
    const qreal w2 = width * width;
    const qreal worst = std::max(w2 * maxVal / s2, s2 / (w2 * minVal));
    return worst;
}

} // namespace

TreemapView::TreemapView(QWidget *parent)
    : QWidget(parent)
{
    setMouseTracking(true);
    setAttribute(Qt::WA_OpaquePaintEvent, true);
    setMinimumSize(320, 240);
    setFocusPolicy(Qt::StrongFocus);
}

void TreemapView::setRoot(DirSizeNodePtr root)
{
    mRoot = std::move(root);
    mFocus = mRoot.get();
    mPath.clear();
    mHovered = nullptr;
    rebuildTiles();
    update();
}

void TreemapView::applyTheme(const QColor &textColor,
                             const QColor &borderColor,
                             const QColor &backgroundColor)
{
    if (textColor.isValid())       mTextColor = textColor;
    if (borderColor.isValid())     mBorderColor = borderColor;
    if (backgroundColor.isValid()) mBackgroundColor = backgroundColor;
    update();
}

bool TreemapView::drillUp()
{
    if (mPath.isEmpty())
        return false;
    mFocus = mPath.takeLast();
    mHovered = nullptr;
    rebuildTiles();
    update();
    return true;
}

void TreemapView::drillInto(DirSizeNode *node)
{
    if (!node || !node->isDir || node == mFocus)
        return;
    mPath.append(mFocus);
    mFocus = node;
    mHovered = nullptr;
    rebuildTiles();
    update();
}

void TreemapView::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    rebuildTiles();
}

void TreemapView::rebuildTiles()
{
    mTiles.clear();
    if (!mFocus || !mFocus->isDir || mFocus->size <= 0)
        return;

    QVector<DirSizeNode*> children;
    children.reserve(static_cast<int>(mFocus->children.size()));
    qreal totalValue = 0;
    for (auto &c : mFocus->children) {
        if (c->size > 0) {
            children.append(c.get());
            totalValue += c->size;
        }
    }
    std::sort(children.begin(), children.end(),
              [](DirSizeNode *a, DirSizeNode *b) { return a->size > b->size; });

    const QRectF area(rect().adjusted(0, 0, -1, -1));
    if (area.width() <= 0 || area.height() <= 0 || totalValue <= 0)
        return;

    squarify(children, area, totalValue, 0);
}

// Squarified treemap (Bruls, Huijsing, van Wijk 2000). `pendingValue` is the
// sum of the values that still need to fit into `rect`; every recursive call
// keeps the invariant that rect.area() corresponds exactly to pendingValue
// at the current scale, which is what makes the strip math come out clean.
void TreemapView::squarify(const QVector<DirSizeNode*> &items,
                           QRectF rect,
                           qreal pendingValue,
                           int depth)
{
    if (items.isEmpty() || rect.width() <= 1 || rect.height() <= 1 ||
        pendingValue <= 0)
        return;

    QVector<DirSizeNode*> row;
    qreal rowSum = 0;
    qreal shortSide = std::min(rect.width(), rect.height());

    int i = 0;
    while (i < items.size()) {
        DirSizeNode *cand = items[i];
        QVector<DirSizeNode*> withCand = row;
        withCand.append(cand);
        const qreal candSum = rowSum + cand->size;

        const qreal worstBefore = worstAspect(row, static_cast<qint64>(rowSum),
                                              shortSide);
        const qreal worstAfter  = worstAspect(withCand, static_cast<qint64>(candSum),
                                              shortSide);

        if (row.isEmpty() || worstAfter <= worstBefore) {
            row = std::move(withCand);
            rowSum = candSum;
            ++i;
        } else {
            layoutRow(row, rowSum, pendingValue, rect, depth);
            pendingValue -= rowSum;
            row.clear();
            rowSum = 0;
            shortSide = std::min(rect.width(), rect.height());
        }
    }
    if (!row.isEmpty())
        layoutRow(row, rowSum, pendingValue, rect, depth);
}

void TreemapView::layoutRow(const QVector<DirSizeNode*> &row,
                            qreal rowSum,
                            qreal pendingValue,
                            QRectF &remaining,
                            int depth)
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
            Tile t;
            t.rect  = QRectF(strip.x(), y, strip.width(), h);
            t.node  = n;
            t.depth = depth;
            mTiles.append(t);
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
            Tile t;
            t.rect  = QRectF(x, strip.y(), w, strip.height());
            t.node  = n;
            t.depth = depth;
            mTiles.append(t);
            x += w;
        }
        remaining = QRectF(remaining.x(), strip.bottom(),
                           remaining.width(),
                           remaining.height() - strip.height());
    }
}

QColor TreemapView::colourFor(DirSizeNode *node, int depth) const
{
    Q_UNUSED(depth);
    // Saturate/desaturate by leaf vs dir so directories read as warmer.
    const qreal sat = node->isDir ? 0.55 : 0.35;
    const qreal val = node->isDir ? 0.75 : 0.65;
    return stableColor(node->name.isEmpty() ? node->path : node->name, sat, val);
}

TreemapView::Tile *TreemapView::tileAt(const QPointF &pos)
{
    // Iterate in reverse so smaller tiles (which come last) win when nested.
    for (int i = mTiles.size() - 1; i >= 0; --i) {
        if (mTiles[i].rect.contains(pos))
            return &mTiles[i];
    }
    return nullptr;
}

void TreemapView::paintEvent(QPaintEvent * /*event*/)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, false);

    p.fillRect(rect(), mBackgroundColor);

    if (!mFocus) {
        p.setPen(mTextColor);
        p.drawText(rect(), Qt::AlignCenter,
                   tr("No scan loaded. Choose a folder and press Scan."));
        return;
    }

    if (mTiles.isEmpty()) {
        p.setPen(mTextColor);
        p.drawText(rect(), Qt::AlignCenter,
                   tr("This folder is empty."));
        return;
    }

    for (const Tile &t : mTiles) {
        const QColor base = colourFor(t.node, t.depth);
        p.fillRect(t.rect, base);
        p.setPen(QPen(mBorderColor, 1));
        p.drawRect(t.rect.adjusted(0, 0, -1, -1));

        // Tile label: only if there's enough room.
        if (t.rect.width() >= 60 && t.rect.height() >= 22) {
            const QString label = t.node->name + "\n" + formatBytes(t.node->size);
            QRectF textRect = t.rect.adjusted(4, 2, -4, -2);
            p.setPen(mTextColor);
            QFont f = p.font();
            f.setPointSizeF(std::max(8.0, std::min(11.0, textRect.height() / 5.0)));
            p.setFont(f);
            p.drawText(textRect,
                       Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap,
                       label);
        }
    }

    if (mHovered) {
        QPen pen(mTextColor, 2);
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);
        p.drawRect(mHovered->rect.adjusted(1, 1, -2, -2));
    }
}

void TreemapView::mouseMoveEvent(QMouseEvent *event)
{
    Tile *t = tileAt(event->position());
    if (t != mHovered) {
        mHovered = t;
        emit tileHovered(t ? t->node : nullptr);
        update();
    }
    if (t) {
        QToolTip::showText(event->globalPosition().toPoint(),
                           QString("%1\n%2")
                               .arg(t->node->path)
                               .arg(formatBytes(t->node->size)),
                           this);
    } else {
        QToolTip::hideText();
    }
}

void TreemapView::mouseDoubleClickEvent(QMouseEvent *event)
{
    Tile *t = tileAt(event->position());
    if (!t || !t->node)
        return;
    if (t->node->isDir)
        emit drillRequested(t->node);
}

void TreemapView::contextMenuEvent(QContextMenuEvent *event)
{
    Tile *t = tileAt(event->pos());
    if (!t || !t->node)
        return;

    QMenu menu(this);
    QAction *reveal = menu.addAction(tr("Reveal in file manager"));
    QAction *trash  = menu.addAction(tr("Move to trash"));
    QAction *drill  = nullptr;
    if (t->node->isDir)
        drill = menu.addAction(tr("Drill into"));

    QAction *chosen = menu.exec(event->globalPos());
    if (!chosen)
        return;
    if (chosen == reveal)
        emit revealRequested(t->node);
    else if (chosen == trash)
        emit trashRequested(t->node);
    else if (drill && chosen == drill)
        emit drillRequested(t->node);
}

void TreemapView::leaveEvent(QEvent * /*event*/)
{
    if (mHovered) {
        mHovered = nullptr;
        emit tileHovered(nullptr);
        update();
    }
    QToolTip::hideText();
}
