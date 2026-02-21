#include "disk_tile.h"

#include <QPainter>
#include <QPainterPath>
#include <QVBoxLayout>
#include "Managers/app_manager.h"
#include "signal_mapper.h"

DiskTile::DiskTile(const QString &arcColorToken, const QString &trackColorToken, QWidget *parent)
    : QWidget(parent), mArcColorToken(arcColorToken), mTrackColorToken(trackColorToken),
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

    mLblTitle = new QLabel(tr("DISK"), this);
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
}

void DiskTile::setValue(int percent, const QString &usedText, const QString &totalText)
{
    mPercent = qBound(0, percent, 100);
    mUsedText = usedText;
    mTotalText = totalText;
    mLblSubtitle->setText(QString("%1 / %2").arg(usedText, totalText));
    update();
}

void DiskTile::setSubtitle(const QString &text)
{
    mLblSubtitle->setText(text);
}

void DiskTile::setDriveHealth(const QString &driveName, const QString &status, bool healthy)
{
    auto *driveLabel = new QLabel(driveName + ": ", mHealthContainer);
    driveLabel->setObjectName("diskTileSubtitle");

    auto *statusLabel = new QLabel(status, mHealthContainer);

    QSettings *sv = AppManager::ins()->getStyleValues();
    QString healthColor = sv ? sv->value(healthy ? "@successColor" : "@destructiveColor").toString() : (healthy ? "#2ec27e" : "#c01c28");
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

void DiskTile::refreshThemeColors()
{
    QSettings *sv = AppManager::ins()->getStyleValues();
    if (!sv)
        return;

    mArcColor = QColor(sv->value(mArcColorToken).toString());
    mTrackColor = QColor(sv->value(mTrackColorToken).toString());
    mTextColor = QColor(sv->value("@color05").toString());

    for (const HealthEntry &entry : mHealthEntries) {
        QString healthColor = sv->value(entry.healthy ? "@successColor" : "@destructiveColor").toString();
        entry.statusLabel->setStyleSheet(QString("color: %1; font-size: 9pt; font-weight: 600;").arg(healthColor));
    }

    update();
}

void DiskTile::paintEvent(QPaintEvent *event)
{
    QWidget::paintEvent(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    // Calculate donut geometry — centered in the widget, below the title
    int titleBottom = mLblTitle->geometry().bottom() + 8;
    int subtitleTop = mLblSubtitle->geometry().top() - 8;
    int availableHeight = subtitleTop - titleBottom;
    int availableWidth = width() - 28; // 14px margin each side

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

    // Value arc (Qt draws arcs starting from 3 o'clock going CCW, in 1/16th degree units)
    // We want to start from 12 o'clock (90°) going clockwise → negative span
    if (mPercent > 0) {
        QPen arcPen(mArcColor, penWidth, Qt::SolidLine, Qt::RoundCap);
        painter.setPen(arcPen);
        int startAngle = 90 * 16; // 12 o'clock
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
