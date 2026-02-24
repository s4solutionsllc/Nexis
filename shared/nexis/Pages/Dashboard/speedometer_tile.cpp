#include "speedometer_tile.h"

#include <QPainter>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QResizeEvent>
#include <QStyle>
#include <cmath>
#include "Managers/app_manager.h"
#include "signal_mapper.h"

static constexpr double kStartAngle = 150.0;
static constexpr double kSweepAngle = 240.0;
static constexpr double kPi = 3.14159265358979323846;

SpeedometerTile::SpeedometerTile(const QString &title, const QString &colorToken, QWidget *parent)
    : MetricTileBase(title, colorToken, parent),
      mPercent(0),
      mCurrentTrend(Stable)
{
    setObjectName("speedometerTile");
    buildLayout();
    refreshThemeColors();

    connect(SignalMapper::ins(), &SignalMapper::sigChangedAppTheme,
            this, &SpeedometerTile::refreshThemeColors);
}

void SpeedometerTile::buildLayout()
{
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(12, 10, 12, 8);
    mainLayout->setSpacing(4);

    mLblTitle = new QLabel(mTitle, this);
    mLblTitle->setObjectName("metricTileTitle");
    mainLayout->addWidget(mLblTitle);

    mainLayout->addStretch();

    mLblValue = new QLabel("--", this);
    mLblValue->setObjectName("metricTileValue");
    mLblValue->hide();

    mLblSecondaryValue = new QLabel(this);
    mLblSecondaryValue->setObjectName("metricTileSubtitle");
    mLblSecondaryValue->hide();

    auto *footerLayout = new QHBoxLayout();
    footerLayout->setContentsMargins(0, 0, 0, 0);

    mLblSubtitle = new QLabel(this);
    mLblSubtitle->setObjectName("metricTileSubtitle");

    mLblTrend = new QLabel(this);
    mLblTrend->setObjectName("metricTileSubtitle");
    mLblTrend->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    mBtnAction = new QPushButton(this);
    mBtnAction->setObjectName("metricTileAction");
    mBtnAction->setCursor(Qt::PointingHandCursor);
    mBtnAction->setFocusPolicy(Qt::NoFocus);
    mBtnAction->hide();
    mBtnAction->setFixedHeight(22);

    footerLayout->addWidget(mLblSubtitle);
    footerLayout->addStretch();
    footerLayout->addWidget(mLblTrend);
    footerLayout->addWidget(mBtnAction);
    mainLayout->addLayout(footerLayout);

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

void SpeedometerTile::setValue(int percent, const QString &valueText)
{
    mPercent = qBound(0, percent, 100);
    mValueText = valueText;
    update();
}

void SpeedometerTile::addDataPoint(double value)
{
    if (mDataBuffer.size() >= SPARKLINE_SIZE)
        mDataBuffer.removeFirst();
    mDataBuffer.append(value);

    updateTrend();
}

void SpeedometerTile::setSubtitle(const QString &text)
{
    mLblSubtitle->setText(text);
}

void SpeedometerTile::setTrendDirection(TrendDirection dir)
{
    mCurrentTrend = dir;
    mLblTrend->setText(trendText(dir));
}

void SpeedometerTile::setSecondaryValue(const QString &text)
{
    mSecondaryText = text;
    update();
}

void SpeedometerTile::setDisplayMode(DisplayMode mode)
{
    mDisplayMode = mode;
    update();
}

void SpeedometerTile::setQuickAction(const QString &text, std::function<void()> callback)
{
    mBtnAction->setText(text);
    mBtnAction->show();
    mLblTrend->hide();
    QObject::disconnect(mBtnAction, &QPushButton::clicked, nullptr, nullptr);
    connect(mBtnAction, &QPushButton::clicked, this, [callback]() { callback(); });
}

QToolButton *SpeedometerTile::gearButton()
{
    return mGearButton;
}

void SpeedometerTile::setGearVisible(bool visible)
{
    mGearButton->setVisible(visible);
}

void SpeedometerTile::refreshThemeColors()
{
    QSettings *sv = AppManager::ins()->getStyleValues();
    if (!sv)
        return;

    mMetricColor = QColor(sv->value(mColorToken).toString());
    mCardBgColor = QColor(sv->value("@cardBg").toString());
    mTextColor = QColor(sv->value("@tertiaryText").toString());
    mSecondaryTextColor = QColor(sv->value("@color05").toString());
    mGreenColor = QColor(sv->value("@successColor").toString());
    mYellowColor = QColor(sv->value("@warningColor").toString());
    mOrangeColor = QColor(sv->value("@accentColor").toString());
    mRedColor = QColor(sv->value("@destructiveColor").toString());

    QString colorHex = mMetricColor.name();
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

void SpeedometerTile::updateGearIcon()
{
    QString theme = AppManager::ins()->resolveThemeName();
    QString path = QString(":/static/themes/%1/img/sidebar-icons/settings.svg").arg(theme);
    mGearButton->setIcon(QIcon(path));
}

void SpeedometerTile::updateTrend()
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

static QColor interpolateColor(const QColor &a, const QColor &b, double t)
{
    t = qBound(0.0, t, 1.0);
    return QColor(
        a.red()   + static_cast<int>((b.red()   - a.red())   * t),
        a.green() + static_cast<int>((b.green() - a.green()) * t),
        a.blue()  + static_cast<int>((b.blue()  - a.blue())  * t),
        a.alpha() + static_cast<int>((b.alpha() - a.alpha()) * t)
    );
}

QColor SpeedometerTile::gradientColorAt(double fraction) const
{
    fraction = qBound(0.0, fraction, 1.0);
    if (fraction < 0.333)
        return interpolateColor(mGreenColor, mYellowColor, fraction / 0.333);
    if (fraction < 0.666)
        return interpolateColor(mYellowColor, mOrangeColor, (fraction - 0.333) / 0.333);
    return interpolateColor(mOrangeColor, mRedColor, (fraction - 0.666) / 0.334);
}

void SpeedometerTile::paintEvent(QPaintEvent *event)
{
    QWidget::paintEvent(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    int titleHeight = mLblTitle->sizeHint().height() + 10;
    int footerTop = mLblSubtitle->geometry().top() - 6;
    int availableHeight = footerTop - titleHeight;
    int availableWidth = width() - 24;

    int dialSize = qMin(availableWidth, availableHeight);
    if (dialSize < 40)
        return;

    int arcThickness = qMax(dialSize / 12, 4);
    int radius = (dialSize - arcThickness) / 2;
    int centerX = width() / 2;
    int centerY = titleHeight + availableHeight / 2;

    QRectF arcRect(centerX - radius, centerY - radius, radius * 2, radius * 2);

    int segmentCount = 120;
    double segmentSweep = kSweepAngle / segmentCount;

    // Background arc (faded gradient at 30% opacity)
    for (int i = 0; i < segmentCount; ++i) {
        double fraction = static_cast<double>(i) / segmentCount;
        QColor color = gradientColorAt(fraction);
        color.setAlphaF(0.3);

        double angle = kStartAngle - fraction * kSweepAngle;

        QPen pen(color, arcThickness, Qt::SolidLine, Qt::FlatCap);
        painter.setPen(pen);
        painter.drawArc(arcRect, static_cast<int>(angle * 16),
                        static_cast<int>(-segmentSweep * 16));
    }

    // Active arc (full opacity up to current value)
    double activeEnd = static_cast<double>(mPercent) / 100.0;
    int activeSegments = static_cast<int>(activeEnd * segmentCount);

    for (int i = 0; i < activeSegments; ++i) {
        double fraction = static_cast<double>(i) / segmentCount;
        QColor color = gradientColorAt(fraction);

        double angle = kStartAngle - fraction * kSweepAngle;

        QPen pen(color, arcThickness, Qt::SolidLine, Qt::FlatCap);
        painter.setPen(pen);
        painter.drawArc(arcRect, static_cast<int>(angle * 16),
                        static_cast<int>(-segmentSweep * 16));
    }

    // Tick marks and numeric labels
    bool showTickLabels = (mDisplayMode != Normal);
    int tickValues[] = { 0, 25, 50, 75, 100 };
    int tickCount = 5;

    int majorTickLen = qMax(dialSize / 16, 4);
    int tickRadius = radius + arcThickness / 2 + 2;

    painter.setPen(QPen(mSecondaryTextColor, 1.5));

    for (int t = 0; t < tickCount; ++t) {
        double fraction = static_cast<double>(tickValues[t]) / 100.0;
        double angleDeg = kStartAngle - fraction * kSweepAngle;
        double angleRad = angleDeg * kPi / 180.0;

        double outerX = centerX + (tickRadius + majorTickLen) * std::cos(angleRad);
        double outerY = centerY - (tickRadius + majorTickLen) * std::sin(angleRad);
        double innerX = centerX + tickRadius * std::cos(angleRad);
        double innerY = centerY - tickRadius * std::sin(angleRad);

        painter.drawLine(QPointF(innerX, innerY), QPointF(outerX, outerY));

        if (showTickLabels) {
            QString label = QString::number(tickValues[t]);
            int fontSize = qMax(dialSize / 20, 7);
            QFont tickFont = painter.font();
            tickFont.setPixelSize(fontSize);
            painter.setFont(tickFont);

            QFontMetrics fm(tickFont);
            int textW = fm.horizontalAdvance(label);
            int textH = fm.height();

            double labelRadius = tickRadius + majorTickLen + textH / 2 + 2;
            double labelX = centerX + labelRadius * std::cos(angleRad) - textW / 2.0;
            double labelY = centerY - labelRadius * std::sin(angleRad) + textH / 4.0;

            painter.drawText(QPointF(labelX, labelY), label);
        }
    }

    // Needle
    double needleFraction = static_cast<double>(mPercent) / 100.0;
    double needleAngleDeg = kStartAngle - needleFraction * kSweepAngle;
    double needleAngleRad = needleAngleDeg * kPi / 180.0;

    int needleRadius = radius - arcThickness / 2 - 4;
    double needleX = centerX + needleRadius * std::cos(needleAngleRad);
    double needleY = centerY - needleRadius * std::sin(needleAngleRad);

    QPen needlePen(mMetricColor, 2.5, Qt::SolidLine, Qt::RoundCap);
    painter.setPen(needlePen);
    painter.drawLine(QPointF(centerX, centerY), QPointF(needleX, needleY));

    // Pivot dot (outer: metric color, inner: card background)
    int pivotOuter = qMax(dialSize / 16, 5);
    int pivotInner = qMax(pivotOuter * 2 / 3, 3);

    painter.setPen(Qt::NoPen);
    painter.setBrush(mMetricColor);
    painter.drawEllipse(QPointF(centerX, centerY), pivotOuter, pivotOuter);

    painter.setBrush(mCardBgColor);
    painter.drawEllipse(QPointF(centerX, centerY), pivotInner, pivotInner);

    // Percentage text inside the dial, below the pivot
    int pctFontDivisor;
    int secFontDivisor;
    switch (mDisplayMode) {
    case Hero:  pctFontDivisor = 5; secFontDivisor = 9; break;
    case Large: pctFontDivisor = 6; secFontDivisor = 10; break;
    default:    pctFontDivisor = 7; secFontDivisor = 11; break;
    }

    QString pctText = mValueText.isEmpty() ? QString("%1%").arg(mPercent) : mValueText;

    QFont pctFont = font();
    int pctFontSize = qMax(12, dialSize / pctFontDivisor);
    pctFont.setPixelSize(pctFontSize);
    pctFont.setBold(true);
    QFontMetrics pctFm(pctFont);
    int pctH = pctFm.height();

    int innerRadius = radius - arcThickness / 2;
    int textWidth = innerRadius * 2 - 8;

    if (!mSecondaryText.isEmpty()) {
        QFont secFont = font();
        int secFontSize = qMin(qMax(9, dialSize / secFontDivisor), 13);
        secFont.setPixelSize(secFontSize);
        QFontMetrics secFm(secFont);
        int secH = secFm.height();

        int totalH = pctH + 2 + secH;
        int textTop = centerY + pivotOuter + 4;

        QRectF pctRect(centerX - textWidth / 2.0, textTop, textWidth, pctH);
        painter.setPen(mSecondaryTextColor);
        painter.setFont(pctFont);
        painter.drawText(pctRect, Qt::AlignHCenter | Qt::AlignVCenter, pctText);

        QString elidedSec = secFm.elidedText(mSecondaryText, Qt::ElideRight, textWidth);
        QRectF secRect(centerX - textWidth / 2.0, textTop + pctH + 2, textWidth, secH);
        painter.setPen(mTextColor);
        painter.setFont(secFont);
        painter.drawText(secRect, Qt::AlignHCenter | Qt::AlignVCenter, elidedSec);
    } else {
        int textTop = centerY + pivotOuter + 4;
        QRectF pctRect(centerX - textWidth / 2.0, textTop, textWidth, pctH);
        painter.setPen(mSecondaryTextColor);
        painter.setFont(pctFont);
        painter.drawText(pctRect, Qt::AlignHCenter | Qt::AlignVCenter, pctText);
    }
}

void SpeedometerTile::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    mGearButton->move(width() - mGearButton->width() - 10, 8);
}
