// SSO-23862 / SSO-24963: bubble-map (nested circle-packing) visualization
// for the disk-space visualizer.
//
// Two nested levels, lit shapes, hover lift, and a drill cross-fade — mirrors
// the treemap redesign. Geometry lives in BubbleLayout (SSO-24963); this
// class only paints it and hit-tests it. Tree/focus/drill-stack/theme/hover/
// context-menu are owned by DiskMapView (shared with TreemapView and
// SunburstView).

#ifndef BUBBLE_MAP_VIEW_H
#define BUBBLE_MAP_VIEW_H

#include "bubble_layout.h"
#include "disk_map_view.h"

class BubbleMapView : public DiskMapView
{
    Q_OBJECT

public:
    explicit BubbleMapView(QWidget *parent = nullptr);

protected:
    void rebuildLayout() override;
    void paintEvent(QPaintEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void aboutToDrill(DirSizeNode *target, bool drillingIn) override;

private:
    BubbleLayout::Metrics scaledMetrics() const;
    void paintGroup(QPainter &p, const BubbleLayout::Group &g, bool hovered);
    void paintBubble(QPainter &p, const BubbleLayout::Bubble &b, bool hovered);
    void paintLayout(QPainter &p, const BubbleLayout::Result &layout);

    BubbleLayout::Result mLayout;
};

#endif // BUBBLE_MAP_VIEW_H
