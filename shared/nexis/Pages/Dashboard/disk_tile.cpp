#include "disk_tile.h"

#include <QPainter>
#include <QPainterPath>
#include <QResizeEvent>
#include <QStyle>
#include <QVBoxLayout>
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
    auto *layout = buildChrome();
    layout->addStretch(1);    // donut is painted in the body
    appendFooter(layout);     // unified footer band (kept minimal for the donut)
}

void DiskTile::setValue(int percent, const QString &valueText)
{
    mPercent = qBound(0, percent, 100);
    mValueText = valueText.isEmpty() ? QString("%1%").arg(mPercent) : valueText;
    update();
}

void DiskTile::addDataPoint(double)
{
}

void DiskTile::setSubtitle(const QString &text)
{
    setSource(text);
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
    applyChromeForMode(mode);
    update();
}

void DiskTile::setQuickAction(const QString &, std::function<void()>)
{
}

void DiskTile::refreshThemeColors()
{
    QSettings *sv = AppManager::ins()->getStyleValues();
    if (!sv)
        return;

    mArcColor = resolvedColor();
    mTrackColor = QColor(sv->value(mTrackColorToken).toString());
    mTextColor = QColor(sv->value("@color05").toString());
    applyAccentColor(mArcColor);
    updateGearIcon();
    update();
}

void DiskTile::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    repositionGearButton();
}

void DiskTile::paintEvent(QPaintEvent *event)
{
    QWidget::paintEvent(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    if (mDisplayMode == Compact) {
        int top = bodyTop();
        int bottom = bodyBottom();
        QRect valueRect(8, top, width() - 16, qMax(1, bottom - top));
        QFont vf = font();
        vf.setPixelSize(qMax(16, valueRect.height() / 2));
        vf.setBold(true);
        painter.setFont(vf);
        painter.setPen(mTextColor);
        painter.drawText(valueRect, Qt::AlignCenter, mValueText);
        return;
    }

    int titleBottom = bodyTop();
    int subtitleTop = bodyBottom();
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

    QString percentText = mValueText.isEmpty() ? QString("%1%").arg(mPercent) : mValueText;
    QRectF textRect = arcRect.adjusted(penWidth, penWidth, -penWidth, -penWidth);
    painter.drawText(textRect, Qt::AlignCenter, percentText);
}
