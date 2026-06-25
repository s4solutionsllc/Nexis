#include "ring_tile.h"

#include <QPainter>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QResizeEvent>
#include <QProgressBar>
#include "Managers/app_manager.h"
#include "signal_mapper.h"

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
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(12, 10, 12, 8);
    mainLayout->setSpacing(4);

    mLblTitle = new QLabel(mTitle, this);
    mLblTitle->setObjectName("metricTileTitle");
    mainLayout->addWidget(mLblTitle);

    // Percentage and secondary value are painted inside the ring, but we still
    // need QLabel instances for text measurement and theme styling.  They are
    // hidden from the layout; paintEvent draws them manually.
    mLblPercentage = new QLabel("--", this);
    mLblPercentage->setObjectName("metricTileValue");
    mLblPercentage->hide();

    mLblSecondaryValue = new QLabel(this);
    mLblSecondaryValue->setObjectName("metricTileSubtitle");
    mLblSecondaryValue->hide();

    // Reserve space for the ring (painted in paintEvent)
    mainLayout->addStretch(1);

    // Thin progress bar below the ring
    mProgressBar = new QProgressBar(this);
    mProgressBar->setObjectName("metricTileProgress");
    mProgressBar->setRange(0, 100);
    mProgressBar->setValue(0);
    mProgressBar->setTextVisible(false);
    mProgressBar->setFixedHeight(4);
    mainLayout->addWidget(mProgressBar);

    mainLayout->addSpacing(2);

    createFooterLayout(mainLayout);

    createGearButton();
}

void RingTile::setValue(int percent, const QString &valueText)
{
    mPercent = qBound(0, percent, 100);
    mProgressBar->setValue(mPercent);
    mLblPercentage->setText(valueText);
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
    mLblSubtitle->setText(text);
}

void RingTile::setTrendDirection(TrendDirection dir)
{
    mCurrentTrend = dir;
    mLblTrend->setText(trendText(dir));
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
    update();
}

void RingTile::setSecondaryValue(const QString &text)
{
    mLblSecondaryValue->setText(text);
    update();
}

void RingTile::refreshThemeColors()
{
    QSettings *sv = AppManager::ins()->getStyleValues();
    if (!sv)
        return;

    mMetricColor = resolvedColor();
    mTrackColor  = QColor(sv->value("@color02").toString());
    mSecondaryTextColor = QColor(sv->value("@tertiaryText").toString());

    QString colorHex = mMetricColor.name();

    mProgressBar->setStyleSheet(
        QString("QProgressBar#metricTileProgress::chunk { background-color: %1; border-radius: 2; }").arg(colorHex));

    applyActionButtonStyle(mMetricColor, QColor(sv->value("@color07").toString()));

    updateGearIcon();
    update();
}

void RingTile::paintEvent(QPaintEvent *event)
{
    QWidget::paintEvent(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    if (mDisplayMode == Compact) {
        int top = mLblTitle->geometry().bottom() + 4;
        int bottom = mProgressBar->isVisible() ? mProgressBar->geometry().top() - 4 : height() - 6;
        QRect valueRect(8, top, width() - 16, qMax(1, bottom - top));
        QFont vf = font();
        vf.setPixelSize(qMax(16, valueRect.height() / 2));
        vf.setBold(true);
        painter.setFont(vf);
        painter.setPen(mMetricColor);
        painter.drawText(valueRect, Qt::AlignCenter, mLblPercentage->text());
        return;
    }

    // Determine the available rect for the ring, between the title and the
    // progress bar.  Title occupies roughly the first 30px, footer+bar ~40px.
    int topMargin    = mLblTitle->geometry().bottom() + 6;
    int bottomMargin = mProgressBar->geometry().top() - 6;
    int availableH   = bottomMargin - topMargin;
    if (availableH < 20)
        return;

    int thickness = ringThickness();
    int diameter  = qMin(width() - 24, availableH) - thickness;
    if (diameter < 20)
        return;

    // Center the ring in the available area
    int cx = width() / 2;
    int cy = topMargin + availableH / 2;
    QRectF ringRect(cx - diameter / 2.0, cy - diameter / 2.0, diameter, diameter);

    // --- Track (full circle) ---
    QPen trackPen(mTrackColor, thickness, Qt::SolidLine, Qt::RoundCap);
    painter.setPen(trackPen);
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(ringRect);

    // --- Value arc (from 12 o'clock, clockwise) ---
    if (mPercent > 0) {
        QPen arcPen(mMetricColor, thickness, Qt::SolidLine, Qt::RoundCap);
        painter.setPen(arcPen);

        // Qt angles: 0 = 3 o'clock, positive = counter-clockwise.
        // 12 o'clock = 90 degrees.  Clockwise span = negative.
        int startAngle = 90 * 16;
        int spanAngle  = -static_cast<int>(mPercent * 3.60) * 16;
        painter.drawArc(ringRect, startAngle, spanAngle);
    }

    // --- Percentage text centered inside ring ---
    QString pctText = mLblPercentage->text();
    int pctFontSize = ringFontSize();
    QFont pctFont = font();
    pctFont.setPixelSize(pctFontSize);
    pctFont.setBold(true);
    painter.setFont(pctFont);
    painter.setPen(mMetricColor);

    QString secText = mLblSecondaryValue->text();
    int secFontSz = secondaryFontSize();

    if (secText.isEmpty()) {
        // Single line centered
        QRectF textRect = ringRect.adjusted(thickness, thickness, -thickness, -thickness);
        painter.drawText(textRect, Qt::AlignCenter, pctText);
    } else {
        // Two lines: percentage above center, secondary below
        QFontMetrics pctFm(pctFont);
        int pctH = pctFm.height();

        QFont secFont = font();
        secFont.setPixelSize(secFontSz);
        QFontMetrics secFm(secFont);
        int secH = secFm.height();

        int totalH = pctH + 2 + secH;
        double textTop = cy - totalH / 2.0;

        QRectF pctRect(ringRect.left() + thickness, textTop, ringRect.width() - 2 * thickness, pctH);
        painter.drawText(pctRect, Qt::AlignHCenter | Qt::AlignVCenter, pctText);

        painter.setFont(secFont);
        painter.setPen(mSecondaryTextColor);

        QRectF secRect(ringRect.left() + thickness, textTop + pctH + 2, ringRect.width() - 2 * thickness, secH);
        painter.drawText(secRect, Qt::AlignHCenter | Qt::AlignVCenter, secText);
    }
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

int RingTile::ringFontSize() const
{
    switch (mDisplayMode) {
    case Hero:  return 28;
    case Large: return 22;
    default:    return 18;
    }
}

int RingTile::secondaryFontSize() const
{
    switch (mDisplayMode) {
    case Hero:  return 13;
    case Large: return 11;
    default:    return 10;
    }
}
