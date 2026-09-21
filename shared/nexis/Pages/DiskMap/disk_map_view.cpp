#include "disk_map_view.h"

#include <QAction>
#include <QContextMenuEvent>
#include <QMenu>
#include <QResizeEvent>

namespace {

// Stable colour pick — keeps the same shape colour across re-layouts (and
// across mode switches, since it's keyed on name, not geometry) so the eye
// can follow a "hot" directory while drilling or switching visualizations.
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

} // namespace

DiskMapView::DiskMapView(QWidget *parent)
    : QWidget(parent)
{
    setMouseTracking(true);
    setAttribute(Qt::WA_OpaquePaintEvent, true);
    setMinimumSize(320, 240);
    setFocusPolicy(Qt::StrongFocus);
}

void DiskMapView::setRoot(DirSizeNodePtr root)
{
    mRoot = std::move(root);
    mFocus = mRoot.get();
    mPath.clear();
    mHoveredNode = nullptr;
    rebuildLayout();
    update();
}

void DiskMapView::applyTheme(const QColor &textColor,
                             const QColor &borderColor,
                             const QColor &backgroundColor)
{
    if (textColor.isValid())       mTextColor = textColor;
    if (borderColor.isValid())     mBorderColor = borderColor;
    if (backgroundColor.isValid()) mBackgroundColor = backgroundColor;
    update();
}

bool DiskMapView::drillUp()
{
    if (mPath.isEmpty())
        return false;
    mFocus = mPath.takeLast();
    mHoveredNode = nullptr;
    rebuildLayout();
    update();
    return true;
}

void DiskMapView::drillInto(DirSizeNode *node)
{
    if (!node || !node->isDir || node == mFocus)
        return;
    mPath.append(mFocus);
    mFocus = node;
    mHoveredNode = nullptr;
    rebuildLayout();
    update();
}

void DiskMapView::resizeEvent(QResizeEvent *event)
{
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

QColor DiskMapView::colourFor(DirSizeNode *node)
{
    // Saturate/desaturate by leaf vs dir so directories read as warmer.
    const qreal sat = node->isDir ? 0.55 : 0.35;
    const qreal val = node->isDir ? 0.75 : 0.65;
    return stableColor(node->name.isEmpty() ? node->path : node->name, sat, val);
}
