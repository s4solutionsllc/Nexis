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
#include <QHash>
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
    /// view (the page holds the shared_ptr).
    void setRoot(DirSizeNodePtr root);

    /// Current focus node (may be a descendant of the original root after
    /// drill-down). Always non-null while the view is showing data.
    DirSizeNode *focus() const { return mFocus; }

    /// Drill one level back up, if possible.
    bool drillUp();

    /// True iff drillUp() would actually move (i.e. the path stack is
    /// non-empty). Used by the page to enable/disable the Up button.
    bool canDrillUp() const { return !mPath.isEmpty(); }

    /// Drill down into a specific node (must be a child of the current
    /// focus). No-op if it isn't a directory.
    void drillInto(DirSizeNode *node);

    /// Apply text/border/background colours and the hue palette fetched from
    /// the active theme. Called by the page when the theme changes.
    void applyTheme(const QColor &textColor,
                    const QColor &borderColor,
                    const QColor &backgroundColor,
                    const QVector<QColor> &palette);

    /// Pure WCAG contrast pick: returns whichever of `optionA` / `optionB`
    /// has the higher contrast ratio against `fill` (relative luminance from
    /// linearised sRGB; contrast = (L1+0.05)/(L2+0.05)). Free of instance
    /// state, so it's public and directly unit-testable.
    static QColor higherContrastColour(const QColor &fill, const QColor &optionA, const QColor &optionB);

signals:
    /// Emitted when the user hovers a shape so the page status bar can
    /// echo the path/size. node may be nullptr if nothing is under the
    /// cursor.
    void tileHovered(DirSizeNode *node);

    /// User double-clicked (or pressed Enter on) a directory shape — the
    /// page should call drillInto().
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

    /// Called right before the focus changes in drillInto()/drillUp(), with
    /// the node being entered (drillingIn = true) or left (drillingIn =
    /// false). Subclasses that animate the transition use this hook to
    /// capture the outgoing layout before assignHues()/rebuildLayout() run.
    virtual void aboutToDrill(DirSizeNode *target, bool drillingIn) { Q_UNUSED(target); Q_UNUSED(drillingIn); }

    /// Called at the top of setRoot(), before the tree is replaced.
    /// Subclasses that animate transitions use this hook to cancel any
    /// in-flight animation so a rescan landing mid-zoom doesn't leave a
    /// stale cross-fade and dead input.
    virtual void rootAboutToChange() {}

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

    /// Label colour to paint on top of `fill` — whichever of the theme's
    /// text/background colours reads better there. Use this instead of
    /// mTextColor for any label drawn directly on a coloured shape (tile,
    /// frame header strip, bubble, wedge); mTextColor alone is dark-on-dark
    /// or light-on-light against roughly half the hue palette depending on
    /// theme.
    QColor labelColourOn(const QColor &fill) const;

    /// Per-node colour, derived from the theme hue palette assigned to its
    /// top-level ancestor by assignHues() (directories read warmer than
    /// files) so the same subtree keeps the same colour across mode
    /// switches.
    QColor colourFor(DirSizeNode *node) const;

    DirSizeNodePtr        mRoot;      ///< keeps the tree alive
    DirSizeNode           *mFocus = nullptr;
    QVector<DirSizeNode*>  mPath;     ///< drill stack (excluding focus)
    DirSizeNode           *mHoveredNode = nullptr;

    QColor mTextColor;
    QColor mBorderColor;
    QColor mBackgroundColor;
    QVector<QColor> mPalette;

private:
    struct HueSlot { int hue = 0; int depth = 0; int rank = 0; };
    QHash<const DirSizeNode*, HueSlot> mHueSlots;
    void assignHues();
};

#endif // DISK_MAP_VIEW_H
