#include "disk_tile.h"

#include <QPainter>
#include <QPainterPath>
#include <QVBoxLayout>

DiskTile::DiskTile(const QColor &arcColor, const QColor &trackColor, QWidget *parent)
    : QWidget(parent), mArcColor(arcColor), mTrackColor(trackColor),
      mPercent(0)
{
    setObjectName("diskTile");
    setMinimumSize(140, 160);
    buildLayout();
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
    painter.setPen(palette().color(QPalette::WindowText));

    QString percentText = QString("%1%").arg(mPercent);
    QRectF textRect = arcRect.adjusted(penWidth, penWidth, -penWidth, -penWidth);
    painter.drawText(textRect, Qt::AlignCenter, percentText);
}
