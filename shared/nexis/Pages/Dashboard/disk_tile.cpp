#include "disk_tile.h"

#include <QPainter>
#include <QPainterPath>
#include <QVBoxLayout>
#include <QResizeEvent>
#include "Managers/app_manager.h"
#include "signal_mapper.h"

DiskTile::DiskTile(const QString &arcColorToken, const QString &trackColorToken, QWidget *parent)
    : MetricTileBase(tr("DISK"), arcColorToken, parent),
      mTrackColorToken(trackColorToken),
      mPercent(0)
{
    setObjectName("diskTile");
    setMinimumSize(140, 160);
    buildLayout();
    refreshThemeColors();

    connect(SignalMapper::ins(), &SignalMapper::sigChangedAppTheme, this, &DiskTile::refreshThemeColors);
}

void DiskTile::buildLayout()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(14, 10, 14, 10);
    layout->setSpacing(4);

    mLblTitle = new QLabel(mTitle, this);
    mLblTitle->setObjectName("diskTileTitle");
    layout->addWidget(mLblTitle);

    // Donut chart area is painted in paintEvent — reserve space
    layout->addStretch(1);

    mLblSubtitle = new QLabel(this);
    mLblSubtitle->setObjectName("diskTileSubtitle");
    mLblSubtitle->setAlignment(Qt::AlignCenter);
    layout->addWidget(mLblSubtitle);

    // Drive health container (hidden until populated)
    mHealthContainer = new QWidget(this);
    mHealthLayout = new QHBoxLayout(mHealthContainer);
    mHealthLayout->setContentsMargins(0, 4, 0, 0);
    mHealthLayout->setSpacing(12);
    mHealthContainer->hide();
    layout->addWidget(mHealthContainer);

    // Gear button for disk selection (positioned absolutely, not in layout)
    mGearButton = new QToolButton(this);
    mGearButton->setObjectName("btnDiskGear");
    mGearButton->setFixedSize(24, 24);
    mGearButton->setIconSize(QSize(14, 14));
    mGearButton->setAutoRaise(true);
    mGearButton->setCursor(Qt::PointingHandCursor);
    mGearButton->setFocusPolicy(Qt::NoFocus);
    mGearButton->hide();
    mGearButton->raise();
    mGearButton->move(width() - mGearButton->width() - 10, 8);
}

void DiskTile::setDiskInfo(int percent, const QString &usedText, const QString &totalText)
{
    mPercent = qBound(0, percent, 100);
    mUsedText = usedText;
    mTotalText = totalText;
    mLblSubtitle->setText(QString("%1 / %2").arg(usedText, totalText));
    update();
}

void DiskTile::setValue(int percent, const QString &)
{
    mPercent = qBound(0, percent, 100);
    update();
}

void DiskTile::addDataPoint(double)
{
}

void DiskTile::setSubtitle(const QString &text)
{
    mLblSubtitle->setText(text);
}

void DiskTile::setTrendDirection(TrendDirection)
{
}

void DiskTile::setSecondaryValue(const QString &)
{
}

void DiskTile::setDisplayMode(DisplayMode mode)
{
    mDisplayMode = mode;
}

void DiskTile::setQuickAction(const QString &, std::function<void()>)
{
}

void DiskTile::setDriveHealth(const QString &driveName, const QString &status, int healthPercent, bool healthy)
{
    auto *driveLabel = new QLabel(driveName + ": ", mHealthContainer);
    driveLabel->setObjectName("diskTileSubtitle");

    QString statusText = status;
    if (healthPercent >= 0)
        statusText += QString(" (%1%)").arg(healthPercent);

    auto *statusLabel = new QLabel(statusText, mHealthContainer);

    QSettings *sv = AppManager::ins()->getStyleValues();
    QString healthColor = sv ? sv->value(healthy ? "@successColor" : "@destructiveColor").toString() : (healthy ? "#2ec27e" : "#E05454");
    statusLabel->setStyleSheet(QString("color: %1; font-size: 9pt; font-weight: 600;").arg(healthColor));

    mHealthEntries.append({statusLabel, healthy});

    auto *pair = new QHBoxLayout();
    pair->setContentsMargins(0, 0, 0, 0);
    pair->setSpacing(2);
    pair->addWidget(driveLabel);
    pair->addWidget(statusLabel);
    mHealthLayout->addLayout(pair);

    mHealthContainer->show();
}

void DiskTile::clearDriveHealth()
{
    mHealthEntries.clear();

    while (QLayoutItem *item = mHealthLayout->takeAt(0)) {
        if (QLayout *childLayout = item->layout()) {
            while (QLayoutItem *sub = childLayout->takeAt(0)) {
                delete sub->widget();
                delete sub;
            }
        }
        delete item->widget();
        delete item;
    }

    mHealthContainer->hide();
}

void DiskTile::refreshThemeColors()
{
    QSettings *sv = AppManager::ins()->getStyleValues();
    if (!sv)
        return;

    mArcColor = resolvedColor();
    mTrackColor = QColor(sv->value(mTrackColorToken).toString());
    mTextColor = QColor(sv->value("@color05").toString());

    for (const HealthEntry &entry : mHealthEntries) {
        QString healthColor = sv->value(entry.healthy ? "@successColor" : "@destructiveColor").toString();
        entry.statusLabel->setStyleSheet(QString("color: %1; font-size: 9pt; font-weight: 600;").arg(healthColor));
    }

    updateGearIcon();
    update();
}

void DiskTile::updateGearIcon()
{
    QString theme = AppManager::ins()->resolveThemeName();
    QString path = QString(":/static/themes/%1/img/sidebar-icons/settings.svg").arg(theme);
    mGearButton->setIcon(QIcon(path));
}

QToolButton *DiskTile::gearButton()
{
    return mGearButton;
}

void DiskTile::setGearVisible(bool visible)
{
    mGearButton->setVisible(visible);
}

void DiskTile::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    mGearButton->move(width() - mGearButton->width() - 10, 8);
}

void DiskTile::paintEvent(QPaintEvent *event)
{
    QWidget::paintEvent(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    int titleBottom = mLblTitle->geometry().bottom() + 8;
    int subtitleTop = mLblSubtitle->geometry().top() - 8;
    int availableHeight = subtitleTop - titleBottom;
    int availableWidth = width() - 28;

    int diameter = qMin(availableWidth, availableHeight);
    if (diameter < 40)
        return;

    int centerX = width() / 2;
    int centerY = titleBottom + availableHeight / 2;

    QRectF donutRect(centerX - diameter / 2, centerY - diameter / 2,
                     diameter, diameter);

    int penWidth = qMax(8, diameter / 10);
    QRectF arcRect = donutRect.adjusted(penWidth / 2, penWidth / 2,
                                        -penWidth / 2, -penWidth / 2);

    // Background track
    QPen trackPen(mTrackColor, penWidth, Qt::SolidLine, Qt::RoundCap);
    painter.setPen(trackPen);
    painter.drawArc(arcRect, 0, 360 * 16);

    // Value arc
    if (mPercent > 0) {
        QPen arcPen(mArcColor, penWidth, Qt::SolidLine, Qt::RoundCap);
        painter.setPen(arcPen);
        int startAngle = 90 * 16;
        int spanAngle = -static_cast<int>(mPercent * 3.6 * 16);
        painter.drawArc(arcRect, startAngle, spanAngle);
    }

    // Center text: percentage
    painter.setPen(Qt::NoPen);
    QFont boldFont = font();
    int fontSize = qMax(12, diameter / 5);
    boldFont.setPixelSize(fontSize);
    boldFont.setBold(true);
    painter.setFont(boldFont);
    painter.setPen(mTextColor);

    QString percentText = QString("%1%").arg(mPercent);
    QRectF textRect = arcRect.adjusted(penWidth, penWidth, -penWidth, -penWidth);
    painter.drawText(textRect, Qt::AlignCenter, percentText);
}
