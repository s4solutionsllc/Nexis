#include "dashboard_tile_wrapper.h"

#include <QVBoxLayout>
#include <QPainter>
#include <QPen>
#include <QApplication>
#include <QResizeEvent>

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
      mGridColSpan(1),
      mStyleButton(nullptr),
      mRemoveButton(nullptr),
      mStyleMenu(nullptr)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    innerWidget->setParent(this);
    layout->addWidget(innerWidget);

    mStyleButton = new QToolButton(this);
    mStyleButton->setObjectName("btnStyleSelector");
    mStyleButton->setFixedSize(24, 24);
    mStyleButton->setIconSize(QSize(14, 14));
    mStyleButton->setIcon(QIcon(":/static/themes/common/img/style-brush.svg"));
    mStyleButton->setAutoRaise(true);
    mStyleButton->setCursor(Qt::PointingHandCursor);
    mStyleButton->setFocusPolicy(Qt::NoFocus);
    mStyleButton->setToolTip(tr("Change Widget Style"));
    mStyleButton->hide();

    mStyleMenu = new QMenu(this);
    mStyleButton->setMenu(mStyleMenu);
    mStyleButton->setPopupMode(QToolButton::InstantPopup);

    connect(mStyleMenu, &QMenu::triggered, this, [this](QAction *action) {
        QString style = action->data().toString();
        if (style != mCurrentStyle)
            emit styleChangeRequested(this, style);
    });

    mRemoveButton = new QToolButton(this);
    mRemoveButton->setObjectName("btnTileRemove");
    mRemoveButton->setFixedSize(24, 24);
    mRemoveButton->setIconSize(QSize(14, 14));
    mRemoveButton->setIcon(QIcon(":/static/themes/common/img/tile-remove.svg"));
    mRemoveButton->setAutoRaise(true);
    mRemoveButton->setCursor(Qt::PointingHandCursor);
    mRemoveButton->setFocusPolicy(Qt::NoFocus);
    mRemoveButton->setToolTip(tr("Remove Widget"));
    mRemoveButton->hide();

    connect(mRemoveButton, &QToolButton::clicked, this, [this]() {
        emit removeRequested(this);
    });
}

QString DashboardTileWrapper::tileId() const { return mTileId; }
QWidget *DashboardTileWrapper::innerWidget() const { return mInnerWidget; }

void DashboardTileWrapper::setInnerWidget(QWidget *newWidget)
{
    layout()->removeWidget(mInnerWidget);
    mInnerWidget->deleteLater();
    mInnerWidget = newWidget;
    newWidget->setParent(this);
    layout()->addWidget(newWidget);
    mStyleButton->raise();
    mRemoveButton->raise();
}

void DashboardTileWrapper::setEditMode(bool enabled)
{
    mEditMode = enabled;
    if (enabled) {
        setCursor(Qt::OpenHandCursor);
        if (!mStyleMenu->isEmpty())
            mStyleButton->show();
        mRemoveButton->show();
    } else {
        unsetCursor();
        mStyleButton->hide();
        mRemoveButton->hide();
    }
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

QString DashboardTileWrapper::currentStyle() const { return mCurrentStyle; }

void DashboardTileWrapper::setCurrentStyle(const QString &style)
{
    mCurrentStyle = style;

    for (QAction *a : mStyleMenu->actions())
        a->setChecked(a->data().toString() == style);
}

void DashboardTileWrapper::setStyleMenuItems(const QStringList &styles, const QString &current)
{
    mStyleMenu->clear();
    mCurrentStyle = current;

    for (const QString &style : styles) {
        QString displayName = style;
        displayName[0] = displayName[0].toUpper();

        QAction *action = mStyleMenu->addAction(displayName);
        action->setData(style);
        action->setCheckable(true);
        action->setChecked(style == current);
    }
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

void DashboardTileWrapper::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    mStyleButton->move(4, 4);
    mRemoveButton->move(width() - mRemoveButton->width() - 4, 4);
}
