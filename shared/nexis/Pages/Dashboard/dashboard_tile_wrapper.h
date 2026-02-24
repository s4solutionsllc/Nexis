#ifndef DASHBOARD_TILE_WRAPPER_H
#define DASHBOARD_TILE_WRAPPER_H

#include <QWidget>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QToolButton>
#include <QMenu>

class DashboardTileWrapper : public QWidget
{
    Q_OBJECT

public:
    explicit DashboardTileWrapper(const QString &tileId, QWidget *innerWidget, QWidget *parent = nullptr);

    QString tileId() const;
    QWidget *innerWidget() const;
    void setInnerWidget(QWidget *newWidget);

    void setEditMode(bool enabled);
    bool isEditMode() const;

    int gridRow() const;
    int gridCol() const;
    int gridRowSpan() const;
    int gridColSpan() const;
    void setGridPosition(int row, int col, int rowSpan = 1, int colSpan = 1);

    QString currentStyle() const;
    void setCurrentStyle(const QString &style);
    void setStyleMenuItems(const QStringList &styles, const QString &current);

signals:
    void dragStarted(DashboardTileWrapper *wrapper, const QPoint &globalPos);
    void dragMoved(DashboardTileWrapper *wrapper, const QPoint &globalPos);
    void dragFinished(DashboardTileWrapper *wrapper, const QPoint &globalPos);
    void resizeRequested(DashboardTileWrapper *wrapper, int newColSpan, int newRowSpan);
    void styleChangeRequested(DashboardTileWrapper *wrapper, const QString &style);
    void removeRequested(DashboardTileWrapper *wrapper);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    QString mTileId;
    QWidget *mInnerWidget;
    bool mEditMode;
    bool mDragging;
    bool mResizing;
    QPoint mDragStartPos;
    QPoint mDragStartGlobal;

    int mGridRow;
    int mGridCol;
    int mGridRowSpan;
    int mGridColSpan;

    QString mCurrentStyle;
    QToolButton *mStyleButton;
    QToolButton *mRemoveButton;
    QMenu *mStyleMenu;

    static const int DRAG_THRESHOLD = 5;
    static const int RESIZE_HANDLE_SIZE = 16;

    bool isInResizeHandle(const QPoint &pos) const;
};

#endif // DASHBOARD_TILE_WRAPPER_H
