#include "ring_tile.h"

#include <QPainter>
#include <QVBoxLayout>
#include <QResizeEvent>
#include <QProgressBar>
#include "Managers/app_manager.h"
#include "signal_mapper.h"
#include "tile_value_fit.h"

RingTile::RingTile(const QString &title, const QString &colorToken, QWidget *parent)
    : MetricTileBase(title, colorToken, parent),
      mPercent(0)
{
    setObjectName("ringTile");
    buildLayout();
    refreshThemeColors();

    connect(SignalMapper::ins(), &SignalMapper::sigChangedAppTheme, this, &RingTile::refreshThemeColors);
}

void RingTile::buildLayout()
{
    auto *mainLayout = buildChrome();
    mainLayout->addStretch(1);   // body: the ring fills this region

    // Thin progress bar — a secondary readout kept in the footer, not the body.
    mProgressBar = new QProgressBar(this);
    mProgressBar->setObjectName("metricTileProgress");
    mProgressBar->setRange(0, 100);
    mProgressBar->setValue(0);
    mProgressBar->setTextVisible(false);
    mProgressBar->setFixedHeight(4);

    appendFooter(mainLayout, mProgressBar);
}

void RingTile::setValue(int percent, const QString &valueText)
{
    mPercent = qBound(0, percent, 100);
    mProgressBar->setValue(mPercent);
    mValueText = valueText.isEmpty() ? QString("%1%").arg(mPercent) : valueText;
    update();
}

void RingTile::addDataPoint(double value)
{
    if (mDataBuffer.size() >= SPARKLINE_SIZE)
        mDataBuffer.removeFirst();
    mDataBuffer.append(value);

    updateTrend();
}

void RingTile::setSubtitle(const QString &text)
{
    setSource(text);
}

void RingTile::setTrendDirection(TrendDirection dir)
{
    setTrendLabel(dir);
}

void RingTile::setQuickAction(const QString &text, std::function<void()> callback)
{
    mBtnAction->setText(text);
    mBtnAction->show();
    mLblTrend->hide();
    QObject::disconnect(mBtnAction, &QPushButton::clicked, nullptr, nullptr);
    connect(mBtnAction, &QPushButton::clicked, this, [callback]() { callback(); });
}

void RingTile::setDisplayMode(DisplayMode mode)
{
    mDisplayMode = mode;
    applyChromeForMode(mode);
    update();
}

void RingTile::setSecondaryValue(const QString &text)
{
    setHeroSecondary(text);
}

void RingTile::refreshThemeColors()
{
    QSettings *sv = AppManager::ins()->getStyleValues();
    if (!sv)
        return;

    mMetricColor = resolvedColor();
    mTrackColor  = QColor(sv->value("@chartGridColor").toString());
    mSecondaryTextColor = QColor(sv->value("@tertiaryText").toString());
    mTextColor = QColor(sv->value("@color05").toString());

    QString colorHex = mMetricColor.name();

    mProgressBar->setStyleSheet(
        QString("QProgressBar#metricTileProgress::chunk { background-color: %1; border-radius: 2; }").arg(colorHex));

    applyActionButtonStyle(mMetricColor, QColor(sv->value("@color07").toString()));
    applyAccentColor(mMetricColor);

    updateGearIcon();
    update();
}

void RingTile::paintEvent(QPaintEvent *event)
{
    QWidget::paintEvent(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    // The ring fills the body between the header and the footer (the progress
    // bar now lives in the footer); the value is drawn centered in the ring.
    int topMargin    = bodyTop();
    int bottomMargin = bodyBottom();
    int availableH   = bottomMargin - topMargin;
    if (availableH < 20)
        return;

    int thickness = ringThickness();
    int diameter  = qMin(width() - 24, availableH) - thickness;
    if (diameter < 20)
        return;

    int cx = width() / 2;
    int cy = topMargin + availableH / 2;
    QRectF ringRect(cx - diameter / 2.0, cy - diameter / 2.0, diameter, diameter);

    // Track (full circle)
    QPen trackPen(mTrackColor, thickness, Qt::SolidLine, Qt::RoundCap);
    painter.setPen(trackPen);
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(ringRect);

    // Value arc (from 12 o'clock, clockwise)
    if (mPercent > 0) {
        QPen arcPen(mMetricColor, thickness, Qt::SolidLine, Qt::RoundCap);
        painter.setPen(arcPen);

        int startAngle = 90 * 16;
        int spanAngle  = -static_cast<int>(mPercent * 3.60) * 16;
        painter.drawArc(ringRect, startAngle, spanAngle);
    }

    // Primary value centered in the ring (unified anatomy), shrunk to fit the
    // clear inner width so wide values never overlap the ring stroke (GH#214).
    const QString valueText = mValueText.isEmpty() ? QString("%1%").arg(mPercent) : mValueText;
    QFont valueFont = font();
    valueFont.setBold(true);
    const int innerWidth = diameter - thickness - 12;
    valueFont.setPixelSize(TileValueFit::fittedPixelSize(valueFont, valueText,
                                                         innerWidth, qMax(14, diameter / 5)));
    painter.setFont(valueFont);
    painter.setPen(mTextColor);
    painter.drawText(ringRect, Qt::AlignCenter | Qt::TextDontClip, valueText);
}

void RingTile::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    repositionGearButton();
}

int RingTile::ringThickness() const
{
    switch (mDisplayMode) {
    case Hero:  return 14;
    case Large: return 12;
    default:    return 10;
    }
}
