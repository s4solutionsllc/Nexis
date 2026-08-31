// SSO-23862: bubble-map (circle-packing) visualization for the disk-space
// visualizer.
//
// Sibling circles are packed by area (radius ∝ sqrt(size)) using an
// iterative relaxation — cheap, dependency-free, and tight enough for a
// disk map. Tree/focus/drill-stack/theme/hover/context-menu are owned by
// DiskMapView (shared with TreemapView and SunburstView); this class only
// builds and hit-tests circle geometry.

#ifndef BUBBLE_MAP_VIEW_H
#define BUBBLE_MAP_VIEW_H

#include <QPointF>
#include <QVector>

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

private:
    struct Circle {
        QPointF center;
        qreal radius = 0;
        DirSizeNode *node = nullptr;
    };

    void packCircles(const QVector<DirSizeNode*> &items);
    Circle *circleAt(const QPointF &pos);

    QVector<Circle> mCircles;
    Circle         *mHoveredCircle = nullptr;
};

#endif // BUBBLE_MAP_VIEW_H
