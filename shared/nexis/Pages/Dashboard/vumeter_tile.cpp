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
      mPercent(0),
      mCurrentTrend(Stable)
{
    setObjectName("vuMeterTile");
    buildLayout();
    refreshThemeColors();

    connect(SignalMapper::ins(), &SignalMapper::sigChangedAppTheme, this, &VuMeterTile::refreshThemeColors);
}

void VuMeterTile::buildLayout()
{
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(12, 10, 12, 8);
    mainLayout->setSpacing(4);

    // Title
    mLblTitle = new QLabel(mTitle, this);
    mLblTitle->setObjectName("metricTileTitle");
    mainLayout->addWidget(mLblTitle);

    // The main area between the title and footer is split into two columns:
    //   Left:  segmented VU bar (painted in paintEvent)
    //   Right: value, secondary, subtitle, trend (QLabels in a VBoxLayout)
    //
    // We use a single QHBoxLayout with a spacer on the left (bar region is
    // painted underneath) and labels on the right.
    auto *bodyLayout = new QHBoxLayout();
    bodyLayout->setContentsMargins(0, 0, 0, 0);
    bodyLayout->setSpacing(0);

    // Left spacer reserves room for the painted bar + scale labels
    bodyLayout->addSpacing(64);

    // Right column: stacked labels
    auto *statsLayout = new QVBoxLayout();
    statsLayout->setContentsMargins(4, 8, 0, 4);
    statsLayout->setSpacing(2);

    mLblValue = new QLabel("--", this);
    mLblValue->setObjectName("metricTileValue");
    mLblValue->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    statsLayout->addWidget(mLblValue);

    mLblSecondaryValue = new QLabel(this);
    mLblSecondaryValue->setObjectName("metricTileSubtitle");
    mLblSecondaryValue->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    mLblSecondaryValue->hide();
    statsLayout->addWidget(mLblSecondaryValue);

    statsLayout->addStretch(1);

    mLblSubtitle = new QLabel(this);
    mLblSubtitle->setObjectName("metricTileSubtitle");
    mLblSubtitle->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    statsLayout->addWidget(mLblSubtitle);

    mLblTrend = new QLabel(this);
    mLblTrend->setObjectName("metricTileSubtitle");
    mLblTrend->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    statsLayout->addWidget(mLblTrend);

    bodyLayout->addLayout(statsLayout, 1);
    mainLayout->addLayout(bodyLayout, 1);

    // Footer action button (hidden by default, shown via setQuickAction)
    mBtnAction = new QPushButton(this);
    mBtnAction->setObjectName("metricTileAction");
    mBtnAction->setCursor(Qt::PointingHandCursor);
    mBtnAction->setFocusPolicy(Qt::NoFocus);
    mBtnAction->hide();
    mBtnAction->setFixedHeight(22);
    mainLayout->addWidget(mBtnAction);

    // Gear button (positioned absolutely, not in layout)
    mGearButton = new QToolButton(this);
    mGearButton->setObjectName("btnMetricGear");
    mGearButton->setFixedSize(24, 24);
    mGearButton->setIconSize(QSize(14, 14));
    mGearButton->setAutoRaise(true);
    mGearButton->setCursor(Qt::PointingHandCursor);
    mGearButton->setFocusPolicy(Qt::NoFocus);
    mGearButton->hide();
    mGearButton->raise();
    mGearButton->move(width() - mGearButton->width() - 10, 8);
}

void VuMeterTile::setValue(int percent, const QString &valueText)
{
    mPercent = qBound(0, percent, 100);
    mValueText = valueText;
    mLblValue->setText(valueText);
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
    mLblSubtitle->setText(text);
}

void VuMeterTile::setTrendDirection(TrendDirection dir)
{
    mCurrentTrend = dir;
    mLblTrend->setText(trendText(dir));
}

void VuMeterTile::setSecondaryValue(const QString &text)
{
    mLblSecondaryValue->setText(text);
    mLblSecondaryValue->setVisible(!text.isEmpty());
}

void VuMeterTile::setDisplayMode(DisplayMode mode)
{
    mDisplayMode = mode;

    mLblValue->setProperty("heroMode", mode == Hero ? "true" : "false");
    mLblValue->setProperty("largeMode", mode == Large ? "true" : "false");
    mLblValue->style()->unpolish(mLblValue);
    mLblValue->style()->polish(mLblValue);

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

    mSuccessColor     = QColor(sv->value("@successColor").toString());
    mWarningColor     = QColor(sv->value("@warningColor").toString());
    mAccentColor      = QColor(sv->value("@accentColor").toString());
    mDestructiveColor = QColor(sv->value("@destructiveColor").toString());
    mTrackColor       = QColor(sv->value("@color02").toString());
    mTextColor        = QColor(sv->value(mColorToken).toString());
    mSecondaryTextColor = QColor(sv->value("@color07").toString());

    QString colorHex  = mTextColor.name();
    QString hoverText = sv->value("@color07").toString();

    mBtnAction->setStyleSheet(
        "QPushButton#metricTileAction {"
        "  font-size: 8pt;"
        "  padding: 2px 8px;"
        "  border-radius: 10px;"
        "  border: 1px solid " + colorHex + ";"
        "  color: " + colorHex + ";"
        "  background: transparent;"
        "}"
        "QPushButton#metricTileAction:hover {"
        "  background-color: " + colorHex + ";"
        "  color: " + hoverText + ";"
        "}"
    );

    updateGearIcon();
    update();
}

void VuMeterTile::paintEvent(QPaintEvent *event)
{
    QWidget::paintEvent(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    // Determine the vertical region for the bar: between the title bottom and widget bottom margin
    int topY    = mLblTitle->geometry().bottom() + 8;
    int bottomY = height() - (mBtnAction->isVisible() ? mBtnAction->geometry().height() + 12 : 8);
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
    mGearButton->move(width() - mGearButton->width() - 10, 8);
}

void VuMeterTile::updateGearIcon()
{
    QString theme = AppManager::ins()->resolveThemeName();
    QString path = QString(":/static/themes/%1/img/sidebar-icons/settings.svg").arg(theme);
    mGearButton->setIcon(QIcon(path));
}

QToolButton *VuMeterTile::gearButton()
{
    return mGearButton;
}

void VuMeterTile::setGearVisible(bool visible)
{
    mGearButton->setVisible(visible);
}

void VuMeterTile::updateTrend()
{
    if (mDataBuffer.size() < 10) {
        setTrendDirection(Stable);
        return;
    }

    int size = mDataBuffer.size();
    double recentAvg = 0, oldAvg = 0;
    for (int i = size - 5; i < size; ++i)
        recentAvg += mDataBuffer.at(i);
    for (int i = size - 10; i < size - 5; ++i)
        oldAvg += mDataBuffer.at(i);
    recentAvg /= 5.0;
    oldAvg /= 5.0;

    if (oldAvg > 0.001) {
        double diff = (recentAvg - oldAvg) / oldAvg;
        if (diff > 0.05)
            setTrendDirection(Rising);
        else if (diff < -0.05)
            setTrendDirection(Falling);
        else
            setTrendDirection(Stable);
    } else {
        setTrendDirection(recentAvg > 0.5 ? Rising : Stable);
    }
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
    int topY    = mLblTitle->geometry().bottom() + 8;
    int bottomY = height() - (mBtnAction->isVisible() ? mBtnAction->geometry().height() + 12 : 8);
    int barH    = bottomY - topY;

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

int VuMeterTile::valueFontSize() const
{
    switch (mDisplayMode) {
    case Hero:  return 28;
    case Large: return 22;
    default:    return 18;
    }
}

int VuMeterTile::secondaryFontSize() const
{
    switch (mDisplayMode) {
    case Hero:  return 13;
    case Large: return 11;
    default:    return 10;
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
