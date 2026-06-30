#include "gauge_tile.h"

#include <QPainter>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QResizeEvent>
#include <QConicalGradient>
#include "Managers/app_manager.h"
#include "signal_mapper.h"

static constexpr double ARC_SWEEP_DEG = 270.0;
static constexpr double ARC_START_DEG = 225.0;
static constexpr int    QT_ARC_UNIT   = 16;

GaugeTile::GaugeTile(const QString &title, const QString &colorToken, QWidget *parent)
    : MetricTileBase(title, colorToken, parent),
      mPercent(0)
{
    setObjectName("gaugeTile");
    setMinimumSize(140, 180);
    buildLayout();
    refreshThemeColors();

    connect(SignalMapper::ins(), &SignalMapper::sigChangedAppTheme,
            this, &GaugeTile::refreshThemeColors);
}

void GaugeTile::buildLayout()
{
    auto *mainLayout = buildChrome();
    mainLayout->addStretch(1);   // body: the gauge arc is painted here
    appendFooter(mainLayout);
}

void GaugeTile::setValue(int percent, const QString &valueText)
{
    mPercent = qBound(0, percent, 100);
    mValueText = valueText;
    setHeroValue(valueText.isEmpty() ? QString("%1%").arg(mPercent) : valueText);
    update();
}

void GaugeTile::addDataPoint(double value)
{
    if (mDataBuffer.size() >= SPARKLINE_SIZE)
        mDataBuffer.removeFirst();
    mDataBuffer.append(value);

    updateTrend();
}

void GaugeTile::setSubtitle(const QString &text)
{
    setSource(text);
}

void GaugeTile::setTrendDirection(TrendDirection dir)
{
    mCurrentTrend = dir;
    mLblTrend->setText(trendText(dir));
}

void GaugeTile::setSecondaryValue(const QString &text)
{
    mSecondaryText = text;
    update();
}

void GaugeTile::setDisplayMode(DisplayMode mode)
{
    mDisplayMode = mode;
    applyChromeForMode(mode);
    update();
}

void GaugeTile::setQuickAction(const QString &text, std::function<void()> callback)
{
    mBtnAction->setText(text);
    mBtnAction->show();
    mLblTrend->hide();
    QObject::disconnect(mBtnAction, &QPushButton::clicked, nullptr, nullptr);
    connect(mBtnAction, &QPushButton::clicked, this, [callback]() { callback(); });
}

void GaugeTile::refreshThemeColors()
{
    QSettings *sv = AppManager::ins()->getStyleValues();
    if (!sv)
        return;

    mArcColor = resolvedColor();
    mTrackColor = QColor(sv->value("@color02").toString());
    mTextColor = QColor(sv->value("@color05").toString());
    mSecondaryTextColor = QColor(sv->value("@tertiaryText").toString());

    mArcEndColor = mArcColor.lighter(140);

    applyActionButtonStyle(mArcColor, QColor(sv->value("@color07").toString()));
    applyAccentColor(mArcColor);

    updateGearIcon();
    update();
}

void GaugeTile::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    repositionGearButton();
}

void GaugeTile::paintEvent(QPaintEvent *event)
{
    QWidget::paintEvent(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    int thicknessBase;
    switch (mDisplayMode) {
    case Hero:  thicknessBase = 14; break;
    case Large: thicknessBase = 12; break;
    default:    thicknessBase = 10; break;
    }

    int top = bodyTop();
    int bottom = bodyBottom();
    int availableHeight = bottom - top;
    int availableWidth = width() - 28;

    int diameter = qMin(availableWidth, availableHeight);
    if (diameter < 40)
        return;

    int centerX = width() / 2;
    int centerY = top + availableHeight / 2;

    int penWidth = qMax(thicknessBase, diameter / thicknessBase);

    QRectF arcRect(centerX - diameter / 2.0 + penWidth / 2.0,
                   centerY - diameter / 2.0 + penWidth / 2.0,
                   diameter - penWidth,
                   diameter - penWidth);

    // Track arc: full 270-degree sweep from 7-o'clock to 5-o'clock
    QPen trackPen(mTrackColor, penWidth, Qt::SolidLine, Qt::RoundCap);
    painter.setPen(trackPen);
    int startAngle16 = static_cast<int>(ARC_START_DEG * QT_ARC_UNIT);
    int sweepAngle16 = static_cast<int>(-ARC_SWEEP_DEG * QT_ARC_UNIT);
    painter.drawArc(arcRect, startAngle16, sweepAngle16);

    // Value arc with conical gradient fill. The numeric reading lives in the
    // footer (stat-card layout), so the arc itself carries no centered text.
    if (mPercent > 0) {
        double valueSweepDeg = ARC_SWEEP_DEG * mPercent / 100.0;

        QConicalGradient gradient(arcRect.center(), ARC_START_DEG);
        double sweepFraction = valueSweepDeg / 360.0;
        gradient.setColorAt(0.0, mArcColor);
        gradient.setColorAt(qMax(0.001, sweepFraction), mArcEndColor);
        gradient.setColorAt(1.0, mArcColor);

        QPen arcPen(QBrush(gradient), penWidth, Qt::SolidLine, Qt::RoundCap);
        painter.setPen(arcPen);

        int valueSweep16 = static_cast<int>(-valueSweepDeg * QT_ARC_UNIT);
        painter.drawArc(arcRect, startAngle16, valueSweep16);
    }
}
