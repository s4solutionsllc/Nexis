#include "dashboard_tile_wrapper.h"
#include "dashboard_layout_util.h"
#include "utilities.h"

#include <QVBoxLayout>
#include <QPainter>
#include <QPen>
#include <QApplication>
#include <QResizeEvent>
#include <QStyle>
#include <QSettings>
#include "Managers/app_manager.h"
#include "signal_mapper.h"

DashboardTileWrapper::DashboardTileWrapper(const QString &uid, const QString &type,
                                           const QString &input, QWidget *innerWidget,
                                           QWidget *parent)
    : QWidget(parent),
      mTileId(uid),
      mTileType(type),
      mInputKey(input),
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
      mStyleMenu(nullptr),
      mCustomSeparator(nullptr)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    innerWidget->setParent(this);
    layout->addWidget(innerWidget);
    applyDepthTreatment();
    // Tiles are built before the theme loads (style values null at ctor time),
    // so re-apply once the theme is available and whenever it changes.
    connect(SignalMapper::ins(), &SignalMapper::sigChangedAppTheme,
            this, &DashboardTileWrapper::applyDepthTreatment);

    mStyleButton = new QToolButton(this);
    mStyleButton->setObjectName("btnStyleSelector");
    mStyleButton->setFixedSize(24, 24);
    mStyleButton->setIconSize(QSize(14, 14));
    mStyleButton->setIcon(QIcon(":/static/themes/common/img/style-brush.svg"));
    mStyleButton->setAutoRaise(true);
    mStyleButton->setCursor(Qt::PointingHandCursor);
    mStyleButton->setToolTip(tr("Change Widget Style"));
    mStyleButton->hide();

    mStyleMenu = new QMenu(this);
    mStyleButton->setMenu(mStyleMenu);
    mStyleButton->setPopupMode(QToolButton::InstantPopup);

    connect(mStyleMenu, &QMenu::triggered, this, [this](QAction *action) {
        QString data = action->data().toString();
        if (data.startsWith("color::")) {
            QString hex = data.mid(7);
            emit colorChangeRequested(this, hex);
        } else if (data.startsWith("range::")) {
            QString rangeId = data.mid(7);
            emit rangeChangeRequested(this, rangeId);
        } else if (!data.isEmpty() && data != mCurrentStyle) {
            emit styleChangeRequested(this, data);
        }
    });

    mRemoveButton = new QToolButton(this);
    mRemoveButton->setObjectName("btnTileRemove");
    mRemoveButton->setFixedSize(24, 24);
    mRemoveButton->setIconSize(QSize(14, 14));
    mRemoveButton->setIcon(QIcon(":/static/themes/common/img/tile-remove.svg"));
    mRemoveButton->setAutoRaise(true);
    mRemoveButton->setCursor(Qt::PointingHandCursor);
    mRemoveButton->setToolTip(tr("Remove Widget"));
    mRemoveButton->hide();

    connect(mRemoveButton, &QToolButton::clicked, this, [this]() {
        emit removeRequested(this);
    });
}

QString DashboardTileWrapper::tileId() const { return mTileId; }
QString DashboardTileWrapper::tileType() const { return mTileType; }
QString DashboardTileWrapper::inputKey() const { return mInputKey; }
void DashboardTileWrapper::setInputKey(const QString &input) { mInputKey = input; }
QWidget *DashboardTileWrapper::innerWidget() const { return mInnerWidget; }

void DashboardTileWrapper::setInnerWidget(QWidget *newWidget)
{
    layout()->removeWidget(mInnerWidget);
    mInnerWidget->deleteLater();
    mInnerWidget = newWidget;
    newWidget->setParent(this);
    layout()->addWidget(newWidget);
    applyDepthTreatment();
    mStyleButton->raise();
    mRemoveButton->raise();
}

void DashboardTileWrapper::applyDepthTreatment()
{
    if (!mInnerWidget)
        return;

    // Give each tile depth (elevation): the theme's warm elevated-card surface +
    // a soft drop shadow so it lifts off the page. The tile needs
    // WA_StyledBackground for its own QSS box to paint, and inline colors
    // (theme-resolved) because inline stylesheets don't get @token substitution.
    QSettings *sv = AppManager::ins()->getStyleValues();
    QString cardBg = sv ? sv->value("@cardBgElevated").toString() : QString();
    QString border = sv ? sv->value("@borderColor").toString()    : QString();
    if (cardBg.isEmpty()) cardBg = "#FFF8F2";   // light-theme fallback (pre-theme-load)
    if (border.isEmpty()) border = "#D0C9C0";

    mInnerWidget->setAttribute(Qt::WA_StyledBackground, true);
    // Scope the rule to the tile's own object name so it does NOT cascade to the
    // child header/footer/label widgets (a selector-less sheet would box them all).
    mInnerWidget->setStyleSheet(QString(
        "#%1 { background-color: %2; border: 1px solid %3; border-radius: 12px; }")
        .arg(mInnerWidget->objectName(), cardBg, border));

    layout()->setContentsMargins(8, 8, 8, 8);   // room for the drop shadow
    Utilities::addDropShadow(mInnerWidget, 90, 26);
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

    for (QAction *a : mStyleMenu->actions()) {
        QString data = a->data().toString();
        if (!data.startsWith("color::") && !data.startsWith("range::"))
            a->setChecked(data == style);
    }
}

void DashboardTileWrapper::setStyleMenuItems(const QStringList &styles, const QString &current)
{
    mStyleMenu->clear();
    mColorActions.clear();
    mRangeActions.clear();
    mCurrentStyle = current;

    QAction *header = mStyleMenu->addAction(tr("Style"));
    header->setEnabled(false);

    for (const QString &style : styles) {
        QString displayName = style;
        displayName[0] = displayName[0].toUpper();

        QAction *action = mStyleMenu->addAction(displayName);
        action->setData(style);
        action->setCheckable(true);
        action->setChecked(style == current);
    }
}

static QPixmap colorSwatchPixmap(const QColor &color, int size)
{
    QPixmap pm(size, size);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(color);
    p.drawEllipse(1, 1, size - 2, size - 2);
    return pm;
}

static QPixmap defaultSwatchPixmap(int size)
{
    QPixmap pm(size, size);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    QPen pen(QColor(150, 150, 150), 1.5);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    p.drawEllipse(2, 2, size - 4, size - 4);
    p.drawLine(QPointF(size * 0.28, size * 0.72), QPointF(size * 0.72, size * 0.28));
    return pm;
}

void DashboardTileWrapper::clearCustomizationSection()
{
    for (QAction *a : mColorActions)
        mStyleMenu->removeAction(a);
    qDeleteAll(mColorActions);
    mColorActions.clear();

    for (QAction *a : mRangeActions)
        mStyleMenu->removeAction(a);
    qDeleteAll(mRangeActions);
    mRangeActions.clear();

    if (mCustomSeparator) {
        mStyleMenu->removeAction(mCustomSeparator);
        delete mCustomSeparator;
        mCustomSeparator = nullptr;
    }

    mCurrentColor.clear();
    mCurrentRange.clear();
}

void DashboardTileWrapper::setColorMenuItems(const QStringList &colors, const QString &current)
{
    mColorActions.clear();
    mCurrentColor = current;

    mCustomSeparator = mStyleMenu->addSeparator();

    QAction *header = mStyleMenu->addAction(tr("Color"));
    header->setEnabled(false);
    mColorActions.append(header);

    QAction *defaultAction = mStyleMenu->addAction(QIcon(defaultSwatchPixmap(16)), tr("Default"));
    defaultAction->setData("color::");
    defaultAction->setCheckable(true);
    defaultAction->setChecked(current.isEmpty());
    mColorActions.append(defaultAction);

    for (const QString &hex : colors) {
        QAction *action = mStyleMenu->addAction(QIcon(colorSwatchPixmap(QColor(hex), 16)), "");
        action->setData(QString("color::%1").arg(hex));
        action->setCheckable(true);
        action->setChecked(hex.compare(current, Qt::CaseInsensitive) == 0);
        mColorActions.append(action);
    }
}

static QPixmap rangeGradientPixmap(const QList<QColor> &colors, int width, int height)
{
    QPixmap pm(width, height);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);

    QLinearGradient grad(0, 0, width, 0);
    if (colors.size() >= 4) {
        grad.setColorAt(0.0,  colors[0]);
        grad.setColorAt(0.33, colors[1]);
        grad.setColorAt(0.66, colors[2]);
        grad.setColorAt(1.0,  colors[3]);
    }

    p.setPen(Qt::NoPen);
    p.setBrush(grad);
    p.drawRoundedRect(0, 1, width, height - 2, 3, 3);
    return pm;
}

void DashboardTileWrapper::setRangeMenuItems(const QStringList &rangeIds, const QStringList &labels,
                                              const QList<QList<QColor>> &swatches, const QString &current)
{
    mRangeActions.clear();
    mCurrentRange = current;

    mCustomSeparator = mStyleMenu->addSeparator();

    QAction *header = mStyleMenu->addAction(tr("Color Range"));
    header->setEnabled(false);
    mRangeActions.append(header);

    QAction *defaultAction = mStyleMenu->addAction(QIcon(defaultSwatchPixmap(16)), tr("Default"));
    defaultAction->setData("range::");
    defaultAction->setCheckable(true);
    defaultAction->setChecked(current.isEmpty());
    mRangeActions.append(defaultAction);

    for (int i = 0; i < rangeIds.size(); ++i) {
        QPixmap swatch = rangeGradientPixmap(swatches.value(i), 40, 16);
        QAction *action = mStyleMenu->addAction(QIcon(swatch), labels.value(i));
        action->setData(QString("range::%1").arg(rangeIds[i]));
        action->setCheckable(true);
        action->setChecked(rangeIds[i] == current);
        mRangeActions.append(action);
    }
}

void DashboardTileWrapper::setCurrentRange(const QString &rangeId)
{
    mCurrentRange = rangeId;
    for (QAction *a : mRangeActions) {
        QString data = a->data().toString();
        if (data == "range::")
            a->setChecked(rangeId.isEmpty());
        else if (data.startsWith("range::"))
            a->setChecked(data.mid(7) == rangeId);
    }
}

QString DashboardTileWrapper::currentRange() const { return mCurrentRange; }

void DashboardTileWrapper::setCurrentColor(const QString &hex)
{
    mCurrentColor = hex;
    for (QAction *a : mColorActions) {
        QString data = a->data().toString().mid(7);
        if (data.isEmpty())
            a->setChecked(hex.isEmpty());
        else
            a->setChecked(data.compare(hex, Qt::CaseInsensitive) == 0);
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
            // GH#191: the grid is responsive (up to kMaxCols wide) and rows grow
            // as needed, so don't hard-cap the span at 2x2 — that was a leftover
            // from the old fixed 4x4 grid. DashboardPage::onTileResizeRequested()
            // gates the actual resize via regionIsFree() (real column count +
            // occupancy), so anything that doesn't fit is simply rejected.
            int newColSpan = qBound(1, (event->pos().x() + cellWidth / 2) / cellWidth, DashboardLayout::kMaxCols);
            int newRowSpan = qBound(1, (event->pos().y() + cellHeight / 2) / cellHeight, DashboardLayout::kMaxCols);
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
    int x = width() - 4;
    x -= mRemoveButton->width();
    mRemoveButton->move(x, 4);
    x -= mStyleButton->width() + 2;
    mStyleButton->move(x, 4);
}
