#include "bubble_map_view.h"

#include <QContextMenuEvent>
#include <QFontMetricsF>
#include <QMouseEvent>
#include <QPainter>
#include <QRadialGradient>
#include <QThreadPool>
#include <QTimer>
#include <QToolTip>
#include <QtConcurrent>

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

// Every BubbleMapView's async pack jobs share this one dedicated,
// single-thread pool instead of QtConcurrent's default (global) one: it
// caps actual concurrent packing at exactly one job regardless of how many
// distinct requests (e.g. a resize drag walking many aspect buckets) get
// dispatched before earlier ones land, and — just as important — keeps
// those CPU-bound jobs from competing with DirSizeScanner's own worker,
// which uses the global pool (see Managers/dir_size_scanner.cpp). Only
// ever touched from the GUI thread (BubbleMapView::startAsyncPack()), so
// the lazy one-time setMaxThreadCount() below needs no extra guarding.
QThreadPool &bubblePackThreadPool()
{
    static QThreadPool pool;
    static bool configured = false;
    if (!configured) {
        pool.setMaxThreadCount(1);
        configured = true;
    }
    return pool;
}

} // namespace

BubbleMapView::BubbleMapView(QWidget *parent)
    : DiskMapView(parent)
{
    mPackDelayTimer = new QTimer(this);
    mPackDelayTimer->setSingleShot(true);
    connect(mPackDelayTimer, &QTimer::timeout, this, [this] {
        // A landed/superseded request already stopped this timer, so
        // reaching here means the pack this delay was measuring is still
        // running.
        if (!mPendingPack)
            return;
        mShowPackingMessage = true;
        update();
    });
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
    const QRectF area(rect());
    const BubbleLayout::Metrics m = scaledMetrics();

    if (BubbleLayout::packsCached(mFocus, area, m, &mPackCache)) {
        // Fast path: every pack this build needs is already cached, so this
        // is just the cheap affine fit — stay fully synchronous, exactly as
        // before this feature existed.
        mPendingPack = false;
        mShowPackingMessage = false;
        mPackDelayTimer->stop();
        mLayout = BubbleLayout::build(mFocus, area, m, &mPackCache);
        mDisplayedTree = mRoot;
        mDisplayedHueSlots = hueSlotsSnapshot();
        startCrossFadeIfArmed();
        return;
    }

    // Cache miss: hand the expensive circle-packing to a worker thread.
    // Deliberately do NOT touch mLayout/mDisplayedHueSlots here — they keep
    // painting whatever they already held (or the empty-state text, if
    // this is the very first load) until a result actually lands in
    // onPackFinished(), so the view never goes blank while packing.
    mPendingPack = true;
    mPackDelayTimer->start(120);
    dispatchPackIfNeeded(area, m);
}

void BubbleMapView::dispatchPackIfNeeded(const QRectF &area, const BubbleLayout::Metrics &m)
{
    const BubbleLayout::PackKey key = BubbleLayout::packKeyFor(mFocus, area, m);
    if (mPackWorkerTrackedSeq != 0 && mPackWorkerTreeGeneration == mTreeGeneration && mPackWorkerKey == key) {
        // A worker is already packing exactly this request's top-level key
        // (a resize drag re-entering the same aspect bucket, or the
        // lockstep hidden view asking for what the visible one already
        // triggered) — dispatching another would just queue redundant work
        // behind it on bubblePackThreadPool(). onPackFinished() re-derives
        // the live focus/area once that one lands and re-dispatches on its
        // own (through this same function) if it's still not enough, so
        // this always converges without ever running more than one pack
        // job at a time for a given (tree, key).
        return;
    }
    startAsyncPack(area, m, key);
}

void BubbleMapView::startAsyncPack(const QRectF &area, const BubbleLayout::Metrics &m, const BubbleLayout::PackKey &key)
{
    // Keep the whole tree alive for the worker even if setRoot()/a rescan
    // replaces mRoot (and mFocus) on the GUI thread before this finishes —
    // mFocus is always a node inside mRoot's tree, so holding this shared_ptr
    // copy is what keeps `focus` below from dangling.
    const DirSizeNodePtr rootKeepAlive = mRoot;
    DirSizeNode *focus = mFocus;
    // A private copy: the worker fills in whatever this build needs on top
    // of it, but that mutation happens only to its own copy (Qt's
    // copy-on-write detaches on first write) — mPackCache itself is never
    // touched off the GUI thread.
    BubbleLayout::PackCache localCache = mPackCache;

    // dispatchSeq identifies THIS dispatch specifically; mPackWorkerTrackedSeq
    // identifies whichever dispatch is the most recently issued one. A
    // second dispatch for a different key (or tree) started while this one
    // is still running overwrites mPackWorkerTrackedSeq — onPackFinished()
    // below only clears it back to "nothing in flight" when it's still the
    // one that dispatch belongs to, so a superseded dispatch landing late
    // never clobbers the bookkeeping for a newer one still in flight.
    const quint64 dispatchSeq = ++mPackDispatchSeq;
    mPackWorkerTrackedSeq = dispatchSeq;
    mPackWorkerTreeGeneration = mTreeGeneration;
    mPackWorkerKey = key;
    ++mPackDispatchCount;
    const quint64 dispatchTreeGeneration = mTreeGeneration;

    auto *watcher = new QFutureWatcher<BubbleLayout::PackCache>(this);
    connect(watcher, &QFutureWatcher<BubbleLayout::PackCache>::finished, this,
            [this, watcher, dispatchSeq, dispatchTreeGeneration] {
        onPackFinished(watcher, dispatchSeq, dispatchTreeGeneration);
    });
    watcher->setFuture(QtConcurrent::run(&bubblePackThreadPool(), [rootKeepAlive, focus, area, m, localCache]() mutable {
        Q_UNUSED(rootKeepAlive);
        BubbleLayout::build(focus, area, m, &localCache);
        return localCache;
    }));
}

void BubbleMapView::onPackFinished(QFutureWatcher<BubbleLayout::PackCache> *watcher, quint64 dispatchSeq, quint64 dispatchTreeGeneration)
{
    const BubbleLayout::PackCache result = watcher->result();
    watcher->deleteLater();

    // Only clear the "in-flight" bookkeeping if this dispatch is still the
    // one it refers to — see the comment in startAsyncPack().
    if (mPackWorkerTrackedSeq == dispatchSeq)
        mPackWorkerTrackedSeq = 0;

    // The tree this was packed against was replaced (setRoot()/a rescan)
    // while this was packing — rootAboutToChange() may already have
    // cleared mPackCache for an entirely new tree, and this result's keys
    // (raw DirSizeNode*) could even collide with the new tree's own node
    // addresses (heap reuse), so it must never be merged. Per the design,
    // the worker itself was never cancelled; only its result is ignored
    // here.
    if (dispatchTreeGeneration != mTreeGeneration)
        return;

    mPackCache.mergeFrom(result);

    // Re-derive against the *live* focus/area rather than trusting that
    // this landing pack is automatically enough: a drill or a resize into a
    // new aspect bucket can have arrived while this was in flight (and, per
    // dispatchPackIfNeeded()'s coalescing, not dispatched its own worker).
    // Calling build() unguarded here would silently reintroduce a
    // GUI-thread freeze the moment that stops being true, so re-check and
    // hand off to another worker rather than assume.
    const QRectF area(rect());
    const BubbleLayout::Metrics m = scaledMetrics();
    if (!BubbleLayout::packsCached(mFocus, area, m, &mPackCache)) {
        dispatchPackIfNeeded(area, m);
        return;
    }

    mPendingPack = false;
    mShowPackingMessage = false;
    mPackDelayTimer->stop();
    mLayout = BubbleLayout::build(mFocus, area, m, &mPackCache);
    mDisplayedTree = mRoot;
    mDisplayedHueSlots = hueSlotsSnapshot();
    startCrossFadeIfArmed();
    update();
}

void BubbleMapView::aboutToDrill(DirSizeNode *target, bool drillingIn)
{
    Q_UNUSED(target);
    Q_UNUSED(drillingIn);
    armCrossFade();
}

void BubbleMapView::rootAboutToChange()
{
    ++mTreeGeneration;
    // Cached nested/top-level packs are keyed by DirSizeNode* — pointers
    // from the tree being replaced must never be looked up again.
    mPackCache.clear();
}

void BubbleMapView::paintGroup(QPainter &p, const BubbleLayout::Group &g, bool hovered)
{
    if (g.radius < 4)
        return;

    const QColor hue = colourFor(g.node, mDisplayedHueSlots);

    QColor shadow = mBackgroundColor.darker(260);
    // Outer rings fainter, innermost (closest to the membrane's own rim)
    // strongest — a real falloff instead of three passes at one flat alpha.
    constexpr int kShadowSteps = 3;
    for (int i = kShadowSteps; i >= 1; --i) {
        const qreal t = static_cast<qreal>(kShadowSteps - i) / (kShadowSteps - 1);
        shadow.setAlpha(int(8 + t * 14));
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
    const QColor base = colourFor(b.node, mDisplayedHueSlots);

    if (!tiny) {
        QColor shadow = mBackgroundColor.darker(260);
        if (hovered) {
            // Outer rings fainter, innermost strongest — see paintGroup()'s
            // shadow loop for the same falloff.
            constexpr int kShadowSteps = 4;
            for (int i = kShadowSteps; i >= 1; --i) {
                const qreal t = static_cast<qreal>(kShadowSteps - i) / (kShadowSteps - 1);
                shadow.setAlpha(int(9 + t * 17));
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
        // `base` is exactly the gradient's own mid stop (grad.setColorAt(0.45,
        // base) above), so it's already the actual on-screen colour there.
        p.setPen(labelColourOn(base));

        const qreal nameH = QFontMetricsF(f).height();
        const qreal nameDy = full ? 2.0 : -3.0;
        const int nameW = std::max(0, int(chordAt(r, nameDy)) - 6);
        const QString name = p.fontMetrics().elidedText(b.node->name, Qt::ElideRight, nameW);
        p.drawText(QRectF(c.x() - r, c.y() - nameDy - nameH, r * 2, nameH * 2),
                   Qt::AlignHCenter | Qt::AlignVCenter, name);

        if (full) {
            QFont sf = f; sf.setBold(false); sf.setPointSizeF(std::max(8.0, f.pointSizeF() - 1.0));
            p.setFont(sf);
            const qreal sizeH = QFontMetricsF(sf).height();
            const qreal sizeDy = -11.0;
            const int sizeW = std::max(0, int(chordAt(r, sizeDy)) - 6);
            const QString size = p.fontMetrics().elidedText(formatBytes(b.node->size), Qt::ElideRight, sizeW);
            p.drawText(QRectF(c.x() - r, c.y() - sizeDy - sizeH, r * 2, sizeH * 2),
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
    } else if (mShowPackingMessage) {
        p.setPen(mTextColor);
        p.drawText(rect(), Qt::AlignCenter, tr("Laying out…"));
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
    if (mPendingPack) {
        QToolTip::hideText();
        return;
    }

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
    if (mPendingPack)
        return;

    DirSizeNode *node = BubbleLayout::hitTest(mLayout, event->position());
    requestDrillIfDir(node);
}

void BubbleMapView::contextMenuEvent(QContextMenuEvent *event)
{
    if (mPendingPack)
        return;

    DirSizeNode *node = BubbleLayout::hitTest(mLayout, event->pos());
    showContextMenuFor(node, event->globalPos());
}

void BubbleMapView::leaveEvent(QEvent * /*event*/)
{
    setHoveredNode(nullptr);
    QToolTip::hideText();
}
