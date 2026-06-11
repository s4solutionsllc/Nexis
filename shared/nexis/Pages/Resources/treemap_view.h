// SSO-3737 / FW-09: treemap rendering for the built-in disk visualizer.
//
// TreemapView is a lean, dependency-free QWidget that paints a squarified
// treemap (Bruls/Huijsing/van Wijk) of a DirSizeNode subtree. The widget
// owns its hover/drill-down/context-menu interactions but keeps actions
// abstract — the embedding dialog wires them to FileSearchService etc.

#ifndef TREEMAP_VIEW_H
#define TREEMAP_VIEW_H

#include <QWidget>
#include <QRectF>
#include <QString>
#include <QVector>

#include "Managers/dir_size_scanner.h"

class TreemapView : public QWidget
{
    Q_OBJECT

public:
    explicit TreemapView(QWidget *parent = nullptr);

    /// Display this subtree. Cheap reference — the node must outlive the
    /// view (the dialog holds the shared_ptr).
    void setRoot(DirSizeNodePtr root);

    /// Current focus node (may be a descendant of the original root after
    /// drill-down). Always non-null while the view is showing data.
    DirSizeNode *focus() const { return mFocus; }

    /// Drill one level back up, if possible.
    bool drillUp();

    /// True iff drillUp() would actually move (i.e. the path stack is
    /// non-empty). Used by the dialog to enable/disable the Up button.
    bool canDrillUp() const { return !mPath.isEmpty(); }

    /// Drill down into a specific node (must be a child of the current
    /// focus). No-op if it isn't a directory.
    void drillInto(DirSizeNode *node);

    /// Apply text/border colours fetched from the active theme. Called by
    /// the dialog when the theme changes.
    void applyTheme(const QColor &textColor,
                    const QColor &borderColor,
                    const QColor &backgroundColor);

signals:
    /// Emitted when the user hovers a tile so the dialog status bar can
    /// echo the path/size. node may be nullptr if no tile is under the
    /// cursor.
    void tileHovered(DirSizeNode *node);

    /// User double-clicked (or pressed Enter on) a directory tile — the
    /// dialog should call drillInto().
    void drillRequested(DirSizeNode *node);

    /// User asked to reveal this tile in the system file manager.
    void revealRequested(DirSizeNode *node);

    /// User asked to move this tile to trash.
    void trashRequested(DirSizeNode *node);

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
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

    void rebuildTiles();
    void squarify(const QVector<DirSizeNode*> &items,
                  QRectF rect,
                  qreal pendingValue,
                  int depth);
    void layoutRow(const QVector<DirSizeNode*> &row,
                   qreal rowSum,
                   qreal pendingValue,
                   QRectF &remaining,
                   int depth);
    QColor colourFor(DirSizeNode *node, int depth) const;
    Tile *tileAt(const QPointF &pos);

    DirSizeNodePtr      mRoot;       ///< keeps the tree alive
    DirSizeNode        *mFocus = nullptr;
    QVector<DirSizeNode*> mPath;     ///< drill stack (excluding focus)

    QVector<Tile> mTiles;
    Tile         *mHovered = nullptr;

    QColor mTextColor      = QColor(0xee, 0xee, 0xee);
    QColor mBorderColor    = QColor(0x10, 0x10, 0x10);
    QColor mBackgroundColor = QColor(0x1e, 0x1e, 0x1e);
};

#endif // TREEMAP_VIEW_H
