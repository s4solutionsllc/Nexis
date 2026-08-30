// SSO-23862: sunburst (radial) visualization for the disk-space visualizer.
//
// Renders mFocus's children as a single donut ring — wedge angle ∝ size,
// with the centre hole labelled with the focus node itself — rather than a
// multi-level ring stack, so drill-down/hover/context-menu behave exactly
// like TreemapView and BubbleMapView (one level shown at a time, double
// click drills in). Tree/focus/drill-stack/theme/hover/context-menu are
// owned by DiskMapView; this class only builds and hit-tests wedge
// geometry.

#ifndef SUNBURST_VIEW_H
#define SUNBURST_VIEW_H

#include <QPointF>
#include <QVector>

#include "disk_map_view.h"

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

private:
    struct Wedge {
        qreal startDeg = 0;   ///< clockwise from 12 o'clock, [0, 360)
        qreal sweepDeg = 0;
        DirSizeNode *node = nullptr;
    };

    Wedge *wedgeAt(const QPointF &pos);

    QPointF mCenter;
    qreal   mOuterR = 0;
    qreal   mInnerR = 0;

    QVector<Wedge> mWedges;
    Wedge         *mHoveredWedge = nullptr;
};

#endif // SUNBURST_VIEW_H
