#include "dashboard_tile_wrapper.h"

#include <QVBoxLayout>
#include <QPainter>
#include <QPen>
#include <QApplication>

DashboardTileWrapper::DashboardTileWrapper(const QString &tileId, QWidget *innerWidget, QWidget *parent)
    : QWidget(parent),
      mTileId(tileId),
      mInnerWidget(innerWidget),
      mEditMode(false),
      mDragging(false),
      mResizing(false),
      mGridRow(0),
      mGridCol(0),
      mGridRowSpan(1),
      mGridColSpan(1)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    innerWidget->setParent(this);
    layout->addWidget(innerWidget);
}

QString DashboardTileWrapper::tileId() const { return mTileId; }
QWidget *DashboardTileWrapper::innerWidget() const { return mInnerWidget; }

void DashboardTileWrapper::setEditMode(bool enabled)
{
    mEditMode = enabled;
    if (enabled)
        setCursor(Qt::OpenHandCursor);
    else
        unsetCursor();
    update();
}

bool DashboardTileWrapper::isEditMode() const { return mEditMode; }

int DashboardTileWrapper::gridRow() const { return mGridRow; }
int DashboardTileWrapper::gridCol() const { return mGridCol; }
int DashboardTileWrapper::gridRowSpan() const { return mGridRowSpan; }
int DashboardTileWrapper::gridColSpan() const { return mGridColSpan; }

void DashboardTileWrapper::setGridPosition(int row, int col, int rowSpan, int colSpan)
{
    mGridRow = row;
    mGridCol = col;
    mGridRowSpan = rowSpan;
    mGridColSpan = colSpan;
}

bool DashboardTileWrapper::isInResizeHandle(const QPoint &pos) const
{
    QRect handleRect(width() - RESIZE_HANDLE_SIZE, height() - RESIZE_HANDLE_SIZE,
                     RESIZE_HANDLE_SIZE, RESIZE_HANDLE_SIZE);
    return handleRect.contains(pos);
}

void DashboardTileWrapper::mousePressEvent(QMouseEvent *event)
{
    if (!mEditMode || event->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(event);
        return;
    }

    mDragStartPos = event->pos();
    mDragStartGlobal = event->globalPosition().toPoint();

    if (isInResizeHandle(event->pos())) {
        mResizing = true;
        setCursor(Qt::SizeFDiagCursor);
    } else {
        setCursor(Qt::ClosedHandCursor);
    }
}

void DashboardTileWrapper::mouseMoveEvent(QMouseEvent *event)
{
    if (!mEditMode) {
        QWidget::mouseMoveEvent(event);
        return;
    }

    if (!(event->buttons() & Qt::LeftButton))
        return;

    QPoint delta = event->pos() - mDragStartPos;

    if (mResizing) {
        int cellWidth = width() / mGridColSpan;
        int cellHeight = height() / mGridRowSpan;
        if (cellWidth > 0 && cellHeight > 0) {
            int newColSpan = qBound(1, (event->pos().x() + cellWidth / 2) / cellWidth, 2);
            int newRowSpan = qBound(1, (event->pos().y() + cellHeight / 2) / cellHeight, 2);
            if (newColSpan != mGridColSpan || newRowSpan != mGridRowSpan)
                emit resizeRequested(this, newColSpan, newRowSpan);
        }
        return;
    }

    if (!mDragging && delta.manhattanLength() >= DRAG_THRESHOLD) {
        mDragging = true;
        emit dragStarted(this, event->globalPosition().toPoint());
    }

    if (mDragging)
        emit dragMoved(this, event->globalPosition().toPoint());
}

void DashboardTileWrapper::mouseReleaseEvent(QMouseEvent *event)
{
    if (!mEditMode || event->button() != Qt::LeftButton) {
        QWidget::mouseReleaseEvent(event);
        return;
    }

    if (mDragging) {
        mDragging = false;
        emit dragFinished(this, event->globalPosition().toPoint());
    }

    if (mResizing) {
        mResizing = false;
    }

    setCursor(Qt::OpenHandCursor);
}

void DashboardTileWrapper::paintEvent(QPaintEvent *event)
{
    QWidget::paintEvent(event);

    if (!mEditMode)
        return;

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // Dashed border overlay
    QPen pen(QColor(150, 150, 150, 120), 2, Qt::DashLine);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(rect().adjusted(1, 1, -1, -1), 12, 12);

    // Resize grip triangle at bottom-right
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(150, 150, 150, 160));
    QPolygon triangle;
    int s = RESIZE_HANDLE_SIZE;
    triangle << QPoint(width(), height())
             << QPoint(width() - s, height())
             << QPoint(width(), height() - s);
    painter.drawPolygon(triangle);
}
