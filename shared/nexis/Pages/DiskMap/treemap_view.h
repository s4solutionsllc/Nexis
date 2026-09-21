// SSO-3737 / FW-09: treemap rendering for the built-in disk visualizer.
//
// TreemapView paints a squarified treemap (Bruls/Huijsing/van Wijk) of a
// DirSizeNode subtree. Tree/focus/drill-stack/theme/hover/context-menu are
// owned by DiskMapView (SSO-23862) — this class only builds and paints the
// tile/frame geometry (SSO-24963: geometry now lives in TreemapLayout).

#ifndef TREEMAP_VIEW_H
#define TREEMAP_VIEW_H

#include <QRectF>
#include <QVector>

#include "disk_map_view.h"
#include "treemap_layout.h"

class TreemapView : public DiskMapView
{
    Q_OBJECT

public:
    explicit TreemapView(QWidget *parent = nullptr);

protected:
    void rebuildLayout() override;
    void paintEvent(QPaintEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    TreemapLayout::Metrics scaledMetrics() const;
    void paintLayout(QPainter &p, const TreemapLayout::Result &layout, qreal opacity);
    void paintTile(QPainter &p, const QRectF &r, DirSizeNode *node, bool hovered);

    TreemapLayout::Result mLayout;
};

#endif // TREEMAP_VIEW_H
