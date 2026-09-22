// SSO-23862 / SSO-24963: two-ring sunburst (radial) visualization for the
// disk-space visualizer.
//
// Two nested rings, lit wedges, hover push-out, and a drill cross-fade —
// mirrors the treemap/bubble-map redesigns. Geometry lives in SunburstLayout
// (SSO-24963); this class only paints it and hit-tests it. Tree/focus/
// drill-stack/theme/hover/context-menu are owned by DiskMapView (shared with
// TreemapView and BubbleMapView).

#ifndef SUNBURST_VIEW_H
#define SUNBURST_VIEW_H

#include "sunburst_layout.h"
#include "disk_map_view.h"

class QPainterPath;

class SunburstView : public DiskMapView
{
    Q_OBJECT

public:
    explicit SunburstView(QWidget *parent = nullptr);

protected:
    void rebuildLayout() override;
    void paintEvent(QPaintEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void aboutToDrill(DirSizeNode *target, bool drillingIn) override;

public:
    /// Test seams: pure geometry of one wedge (optionally pushed outward by
    /// `radialOffset`) and the chart centre it is drawn around.
    QPainterPath wedgePath(const SunburstLayout::Wedge &w, qreal radialOffset = 0) const;
    QPointF chartCenter() const { return mLayout.center; }

private:
    SunburstLayout::Metrics scaledMetrics() const;
    void paintShadowDisc(QPainter &p);
    void paintWedge(QPainter &p, const SunburstLayout::Wedge &w, bool pushed, bool outlined);
    void paintHub(QPainter &p);

    SunburstLayout::Result mLayout;
};

#endif // SUNBURST_VIEW_H
