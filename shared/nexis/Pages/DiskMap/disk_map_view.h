// SSO-23862: shared base for the disk-space visualizer's interchangeable
// rendering modes (treemap / bubble-map / sunburst).
//
// DiskMapView owns the parts that are identical across every visualization:
// the DirSizeNode tree/focus/drill-stack, theme colours, hover bookkeeping,
// and the reveal/trash/drill-into context menu. Subclasses only implement
// rebuildLayout() (turn mFocus's children into their own geometry — tiles,
// circles, wedges) plus the QWidget paint/mouse handlers needed to hit-test
// that geometry. This keeps all three modes on the exact same scan data and
// interaction contract without re-deriving it per mode.

#ifndef DISK_MAP_VIEW_H
#define DISK_MAP_VIEW_H

#include <QColor>
#include <QString>
#include <QVector>
#include <QWidget>

#include "Managers/dir_size_scanner.h"

class DiskMapView : public QWidget
{
    Q_OBJECT

public:
    explicit DiskMapView(QWidget *parent = nullptr);

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
    /// Emitted when the user hovers a shape so the dialog status bar can
    /// echo the path/size. node may be nullptr if nothing is under the
    /// cursor.
    void tileHovered(DirSizeNode *node);

    /// User double-clicked (or pressed Enter on) a directory shape — the
    /// dialog should call drillInto().
    void drillRequested(DirSizeNode *node);

    /// User asked to reveal this shape in the system file manager.
    void revealRequested(DirSizeNode *node);

    /// User asked to move this shape to trash.
    void trashRequested(DirSizeNode *node);

protected:
    /// Recompute this mode's geometry (tiles/circles/wedges) from mFocus's
    /// children. Called after setRoot()/drillUp()/drillInto() and whenever
    /// the widget resizes.
    virtual void rebuildLayout() = 0;

    void resizeEvent(QResizeEvent *event) override;

    /// Shared hover bookkeeping: emits tileHovered() and repaints only when
    /// the hovered node actually changes. Tooltip text/positioning stays
    /// with the caller since it needs the current global mouse position.
    void setHoveredNode(DirSizeNode *node);
    DirSizeNode *hoveredNode() const { return mHoveredNode; }

    /// Shared reveal/trash/drill-into context menu — identical across all
    /// three visualization modes.
    void showContextMenuFor(DirSizeNode *node, const QPoint &globalPos);

    /// Shared double-click semantics: drill only into directories.
    void requestDrillIfDir(DirSizeNode *node);

    static QString formatBytes(qint64 bytes);

    /// Stable per-node colour (directories read warmer than files) so the
    /// same subtree keeps the same colour across mode switches.
    static QColor colourFor(DirSizeNode *node);

    DirSizeNodePtr        mRoot;      ///< keeps the tree alive
    DirSizeNode           *mFocus = nullptr;
    QVector<DirSizeNode*>  mPath;     ///< drill stack (excluding focus)
    DirSizeNode           *mHoveredNode = nullptr;

    QColor mTextColor       = QColor(0xee, 0xee, 0xee);
    QColor mBorderColor     = QColor(0x10, 0x10, 0x10);
    QColor mBackgroundColor = QColor(0x1e, 0x1e, 0x1e);
};

#endif // DISK_MAP_VIEW_H
