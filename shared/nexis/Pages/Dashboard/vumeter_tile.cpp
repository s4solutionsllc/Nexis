#include "vumeter_tile.h"

#include <QPainter>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QResizeEvent>
#include <QStyle>
#include "Managers/app_manager.h"
#include "signal_mapper.h"

VuMeterTile::VuMeterTile(const QString &title, const QString &colorToken, QWidget *parent)
    : MetricTileBase(title, colorToken, parent),
      mPercent(0)
{
    setObjectName("vuMeterTile");
    buildLayout();
    refreshThemeColors();

    connect(SignalMapper::ins(), &SignalMapper::sigChangedAppTheme, this, &VuMeterTile::refreshThemeColors);
}

void VuMeterTile::buildLayout()
{
    auto *mainLayout = buildChrome();
    mainLayout->addStretch(1);   // body: the segmented VU bar is painted here
    appendFooter(mainLayout);
}

void VuMeterTile::setValue(int percent, const QString &valueText)
{
    mPercent = qBound(0, percent, 100);
    mValueText = valueText;
    setHeroValue(valueText.isEmpty() ? QString("%1%").arg(mPercent) : valueText);
    update();
}

void VuMeterTile::addDataPoint(double value)
{
    if (mDataBuffer.size() >= SPARKLINE_SIZE)
        mDataBuffer.removeFirst();
    mDataBuffer.append(value);

    updateTrend();
}

void VuMeterTile::setSubtitle(const QString &text)
{
    setSource(text);
}

void VuMeterTile::setTrendDirection(TrendDirection dir)
{
    setTrendLabel(dir);
}

void VuMeterTile::setSecondaryValue(const QString &text)
{
    setHeroSecondary(text);
}

void VuMeterTile::setDisplayMode(DisplayMode mode)
{
    mDisplayMode = mode;
    applyChromeForMode(mode);
    update();
}

void VuMeterTile::setQuickAction(const QString &text, std::function<void()> callback)
{
    mBtnAction->setText(text);
    mBtnAction->show();
    QObject::disconnect(mBtnAction, &QPushButton::clicked, nullptr, nullptr);
    connect(mBtnAction, &QPushButton::clicked, this, [callback]() { callback(); });
}

void VuMeterTile::refreshThemeColors()
{
    QSettings *sv = AppManager::ins()->getStyleValues();
    if (!sv)
        return;

    if (!mColorRange.isEmpty()) {
        QList<QColor> rc = rangeColors(mColorRange);
        mSuccessColor     = rc[0];
        mWarningColor     = rc[1];
        mAccentColor      = rc[2];
        mDestructiveColor = rc[3];
    } else {
        mSuccessColor     = QColor(sv->value("@successColor").toString());
        mWarningColor     = QColor(sv->value("@warningColor").toString());
        mAccentColor      = QColor(sv->value("@accentColor").toString());
        mDestructiveColor = QColor(sv->value("@destructiveColor").toString());
    }
    mTrackColor       = QColor(sv->value("@color02").toString());
    mTextColor        = QColor(sv->value(mColorToken).toString());
    mSecondaryTextColor = QColor(sv->value("@tertiaryText").toString());

    applyActionButtonStyle(mTextColor, QColor(sv->value("@color07").toString()));
    applyAccentColor(mTextColor);

    updateGearIcon();
    update();
}

void VuMeterTile::paintEvent(QPaintEvent *event)
{
    QWidget::paintEvent(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    // The segmented bar sits in the body; the numeric reading is in the footer.
    int topY    = bodyTop();
    int bottomY = bodyBottom();
    int barH    = bottomY - topY;
    if (barH < 40)
        return;

    int bw          = barWidth();
    int barLeftX    = 12;
    int segments    = segmentCount();
    int segGap      = 2;
    int segRadius   = 3;
    int segH        = qMax(2, (barH - (segments - 1) * segGap) / segments);
    int actualBarH  = segments * segH + (segments - 1) * segGap;
    int barTopY     = topY + (barH - actualBarH) / 2;

    // Number of filled segments based on percentage
    int filledCount = qRound(mPercent / 100.0 * segments);

    // Paint segments bottom-up
    for (int i = 0; i < segments; ++i) {
        int segIndex = segments - 1 - i; // bottom-up: i=0 is bottom segment
        int segY     = barTopY + segIndex * (segH + segGap);

        QRectF segRect(barLeftX, segY, bw, segH);

        if (i < filledCount) {
            painter.setBrush(segmentColor(i, segments));
        } else {
            painter.setBrush(mTrackColor);
        }

        painter.setPen(Qt::NoPen);
        painter.drawRoundedRect(segRect, segRadius, segRadius);
    }

    // Scale labels beside the bar
    int scaleFontSz = scaleFontSize();
    QFont scaleFont = font();
    scaleFont.setPixelSize(scaleFontSz);
    painter.setFont(scaleFont);
    painter.setPen(mSecondaryTextColor);

    int scaleLabelX = barLeftX + bw + 4;

    struct ScaleLabel { int value; double normalizedPos; };
    ScaleLabel scaleLabels[] = {
        { 100, 1.0 },
        {  66, 0.66 },
        {  33, 0.33 },
        {   0, 0.0 }
    };

    QFontMetrics scaleFm(scaleFont);
    int labelH = scaleFm.height();

    for (const auto &sl : scaleLabels) {
        // Map the normalized position to a Y coordinate within the bar
        // 1.0 = top of bar, 0.0 = bottom of bar
        int labelCenterY = barTopY + actualBarH - static_cast<int>(sl.normalizedPos * actualBarH);
        int labelY       = labelCenterY - labelH / 2;

        painter.drawText(scaleLabelX, labelY, 28, labelH,
                         Qt::AlignLeft | Qt::AlignVCenter,
                         QString::number(sl.value));
    }
}

void VuMeterTile::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    repositionGearButton();
}

QColor VuMeterTile::segmentColor(int segmentIndex, int totalSegments) const
{
    // Color is based on the segment's POSITION in the bar, not the fill level.
    // Bottom segments = green, top segments = red.
    double normalizedPos = static_cast<double>(segmentIndex) / qMax(1, totalSegments - 1);

    if (normalizedPos < 0.40)
        return mSuccessColor;
    else if (normalizedPos < 0.60)
        return mWarningColor;
    else if (normalizedPos < 0.80)
        return mAccentColor;
    else
        return mDestructiveColor;
}

int VuMeterTile::segmentCount() const
{
    // Adapt segment count to available height, clamped to [8, 20]
    int barH = bodyBottom() - bodyTop();

    int count = qBound(8, barH / 10, 20);

    switch (mDisplayMode) {
    case Hero:
        return qBound(12, count, 20);
    case Large:
        return qBound(10, count, 18);
    default:
        return count;
    }
}

int VuMeterTile::barWidth() const
{
    // In compact (Normal 1x1) mode the bar is wider relative to the tile
    switch (mDisplayMode) {
    case Hero:  return 40;
    case Large: return 36;
    default:    return 32;
    }
}

int VuMeterTile::scaleFontSize() const
{
    switch (mDisplayMode) {
    case Hero:  return 10;
    case Large: return 9;
    default:    return 8;
    }
}
