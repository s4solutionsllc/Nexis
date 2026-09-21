#include "disk_map_view.h"

#include <QAction>
#include <QContextMenuEvent>
#include <QEasingCurve>
#include <QMenu>
#include <QPainter>
#include <QResizeEvent>

#include <algorithm>
#include <cmath>
#include <functional>

#include "utilities.h"

DiskMapView::DiskMapView(QWidget *parent)
    : QWidget(parent)
{
    setMouseTracking(true);
    setAttribute(Qt::WA_OpaquePaintEvent, true);
    setMinimumSize(320, 240);
    setFocusPolicy(Qt::StrongFocus);

    mCrossFade = new QVariantAnimation(this);
    mCrossFade->setDuration(200);
    mCrossFade->setEasingCurve(QEasingCurve::OutCubic);
    mCrossFade->setStartValue(0.0);
    mCrossFade->setEndValue(1.0);
    connect(mCrossFade, &QVariantAnimation::valueChanged, this, [this](const QVariant &v) {
        mCrossFadeT = v.toReal();
        update();
    });
    connect(mCrossFade, &QVariantAnimation::finished, this, [this] {
        mCrossFadeT = 1.0;
        mCrossFadePixmap = QPixmap();
        update();
    });
}

void DiskMapView::setRoot(DirSizeNodePtr root)
{
    rootAboutToChange();
    cancelCrossFade();
    mRoot = std::move(root);
    mFocus = mRoot.get();
    mPath.clear();
    mHoveredNode = nullptr;
    assignHues();
    rebuildLayout();
    update();
}

void DiskMapView::applyTheme(const QColor &textColor,
                             const QColor &borderColor,
                             const QColor &backgroundColor,
                             const QVector<QColor> &palette)
{
    if (textColor.isValid())       mTextColor = textColor;
    if (borderColor.isValid())     mBorderColor = borderColor;
    if (backgroundColor.isValid()) mBackgroundColor = backgroundColor;
    if (!palette.isEmpty())        mPalette = palette;
    update();
}

bool DiskMapView::drillUp()
{
    if (mPath.isEmpty())
        return false;
    aboutToDrill(mFocus, false);
    mFocus = mPath.takeLast();
    mHoveredNode = nullptr;
    assignHues();
    rebuildLayout();
    update();
    return true;
}

void DiskMapView::drillInto(DirSizeNode *node)
{
    if (!node || !node->isDir || node == mFocus)
        return;
    aboutToDrill(node, true);
    mPath.append(mFocus);
    mFocus = node;
    mHoveredNode = nullptr;
    assignHues();
    rebuildLayout();
    update();
}

void DiskMapView::resizeEvent(QResizeEvent *event)
{
    cancelCrossFade();
    QWidget::resizeEvent(event);
    rebuildLayout();
}

void DiskMapView::setHoveredNode(DirSizeNode *node)
{
    if (node == mHoveredNode)
        return;
    mHoveredNode = node;
    emit tileHovered(node);
    update();
}

void DiskMapView::requestDrillIfDir(DirSizeNode *node)
{
    if (node && node->isDir)
        emit drillRequested(node);
}

void DiskMapView::showContextMenuFor(DirSizeNode *node, const QPoint &globalPos)
{
    if (!node)
        return;

    QMenu menu(this);
    QAction *reveal = menu.addAction(tr("Reveal in file manager"));
    QAction *trash  = menu.addAction(tr("Move to trash"));
    QAction *drill  = nullptr;
    if (node->isDir)
        drill = menu.addAction(tr("Drill into"));

    QAction *chosen = menu.exec(globalPos);
    if (!chosen)
        return;
    if (chosen == reveal)
        emit revealRequested(node);
    else if (chosen == trash)
        emit trashRequested(node);
    else if (drill && chosen == drill)
        emit drillRequested(node);
}

namespace {
qreal srgbToLinear(qreal c)
{
    return c <= 0.03928 ? c / 12.92 : std::pow((c + 0.055) / 1.055, 2.4);
}

qreal relativeLuminance(const QColor &c)
{
    const qreal r = srgbToLinear(c.redF());
    const qreal g = srgbToLinear(c.greenF());
    const qreal b = srgbToLinear(c.blueF());
    return 0.2126 * r + 0.7152 * g + 0.0722 * b;
}

qreal contrastRatio(const QColor &a, const QColor &b)
{
    const qreal la = relativeLuminance(a), lb = relativeLuminance(b);
    return (std::max(la, lb) + 0.05) / (std::min(la, lb) + 0.05);
}
}

QColor DiskMapView::higherContrastColour(const QColor &fill, const QColor &optionA, const QColor &optionB)
{
    return contrastRatio(fill, optionA) >= contrastRatio(fill, optionB) ? optionA : optionB;
}

QColor DiskMapView::labelColourOn(const QColor &fill) const
{
    return higherContrastColour(fill, mTextColor, mBackgroundColor);
}

QString DiskMapView::formatBytes(qint64 b)
{
    if (b < 1024)
        return QString::number(b) + " B";
    static const char *suffixes[] = {"KiB", "MiB", "GiB", "TiB", "PiB"};
    double v = static_cast<double>(b);
    int i = -1;
    do { v /= 1024.0; ++i; } while (v >= 1024.0 && i < 4);
    return QString::number(v, 'f', v < 10 ? 2 : 1) + " " + suffixes[i];
}

void DiskMapView::assignHues()
{
    mHueSlots.clear();
    if (!mFocus)
        return;
    QVector<DirSizeNode*> tops;
    for (auto &c : mFocus->children)
        tops.append(c.get());
    std::sort(tops.begin(), tops.end(),
              [](DirSizeNode *a, DirSizeNode *b) { return a->size > b->size; });

    std::function<void(DirSizeNode*, int, int)> walk = [&](DirSizeNode *n, int hue, int depth) {
        if (depth > 4)
            return;
        QVector<DirSizeNode*> kids;
        for (auto &c : n->children)
            kids.append(c.get());
        std::sort(kids.begin(), kids.end(),
                  [](DirSizeNode *a, DirSizeNode *b) { return a->size > b->size; });
        for (int i = 0; i < kids.size(); ++i) {
            mHueSlots.insert(kids[i], {hue, depth, i});
            walk(kids[i], hue, depth + 1);
        }
    };
    for (int i = 0; i < tops.size(); ++i) {
        mHueSlots.insert(tops[i], {i, 0, 0});
        walk(tops[i], i, 1);
    }
}

void DiskMapView::armCrossFade()
{
    if (Utilities::prefersReducedMotion() || !isVisible() || size().isEmpty())
        return;

    QPixmap pm(size() * devicePixelRatioF());
    pm.setDevicePixelRatio(devicePixelRatioF());
    if (mBackgroundColor.isValid())
        pm.fill(mBackgroundColor);
    else
        pm.fill(Qt::transparent);
    render(&pm);

    mCrossFadePixmap = pm;
    mCrossFadePending = true;
}

void DiskMapView::startCrossFadeIfArmed()
{
    if (!mCrossFadePending)
        return;
    mCrossFadePending = false;
    if (mCrossFadePixmap.isNull())
        return;
    mCrossFade->stop();
    mCrossFade->start();
}

void DiskMapView::paintCrossFadeOverlay(QPainter &p)
{
    if (mCrossFade->state() != QAbstractAnimation::Running || mCrossFadePixmap.isNull())
        return;
    p.save();
    p.setOpacity(1.0 - mCrossFadeT);
    p.drawPixmap(rect(), mCrossFadePixmap);
    p.restore();
}

bool DiskMapView::isCrossFadeRunning() const
{
    return mCrossFade->state() == QAbstractAnimation::Running;
}

void DiskMapView::cancelCrossFade()
{
    mCrossFade->stop();
    mCrossFadeT = 1.0;
    mCrossFadePixmap = QPixmap();
    mCrossFadePending = false;
}

QColor DiskMapView::colourFor(DirSizeNode *node) const
{
    if (mPalette.isEmpty())
        return mBorderColor;
    const HueSlot s = mHueSlots.value(node);
    QColor c = mPalette[s.hue % mPalette.size()];
    float h, sat, l, a;
    c.getHslF(&h, &sat, &l, &a);
    const float step = 0.045f * std::min(s.rank, 5) + 0.03f * std::max(0, s.depth - 1);
    l = std::clamp(l - step, 0.18f, 0.80f);
    if (!node->isDir)
        sat *= 0.78f;
    c.setHslF(h, sat, l, a);
    return c;
}
