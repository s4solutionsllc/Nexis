// SSO-3737 / FW-09: treemap rendering for the built-in disk visualizer.
//
// TreemapView paints a squarified treemap (Bruls/Huijsing/van Wijk) of a
// DirSizeNode subtree. Tree/focus/drill-stack/theme/hover/context-menu are
// owned by DiskMapView (SSO-23862) — this class only builds and hit-tests
// the tile geometry.

#ifndef TREEMAP_VIEW_H
#define TREEMAP_VIEW_H

#include <QRectF>
#include <QVector>

#include "disk_map_view.h"

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
    struct Tile {
        QRectF rect;
        DirSizeNode *node = nullptr;
        int depth = 0;        ///< 0 == focus's children; deeper for nested
    };

    void squarify(const QVector<DirSizeNode*> &items,
                  QRectF rect,
                  qreal pendingValue,
                  int depth);
    void layoutRow(const QVector<DirSizeNode*> &row,
                   qreal rowSum,
                   qreal pendingValue,
                   QRectF &remaining,
                   int depth);
    Tile *tileAt(const QPointF &pos);

    QVector<Tile> mTiles;
    Tile         *mHoveredTile = nullptr;
};

#endif // TREEMAP_VIEW_H
