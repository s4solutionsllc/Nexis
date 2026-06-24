#include "hybrid_tile.h"

#include <QPainter>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QResizeEvent>
#include "Managers/app_manager.h"
#include "signal_mapper.h"

static constexpr double GAUGE_START_ANGLE = 225.0;
static constexpr double GAUGE_SWEEP_ANGLE = 270.0;
static constexpr int ARC_PEN_WIDTH = 6;

HybridTile::HybridTile(const QString &title, const QString &colorToken, QWidget *parent)
    : MetricTileBase(title, colorToken, parent),
      mPercent(0)
{
    setObjectName("hybridTile");
    buildLayout();
    refreshThemeColors();

    connect(SignalMapper::ins(), &SignalMapper::sigChangedAppTheme, this, &HybridTile::refreshThemeColors);
}

void HybridTile::buildLayout()
{
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(12, 10, 12, 8);
    mainLayout->setSpacing(4);

    // Title
    mLblTitle = new QLabel(mTitle, this);
    mLblTitle->setObjectName("metricTileTitle");
    mainLayout->addWidget(mLblTitle);

    // Gauge area: a transparent widget whose paintEvent draws the arc
    mGaugeArea = new QWidget(this);
    mGaugeArea->setAttribute(Qt::WA_TransparentForMouseEvents);
    mGaugeArea->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    mainLayout->addWidget(mGaugeArea, 1);

    // Mini sparkline chart
    mSeries = new QLineSeries();

    auto *baseline = new QLineSeries();
    for (int i = 0; i < SPARKLINE_SIZE; ++i)
        baseline->append(i, 0);

    mAreaSeries = new QAreaSeries(mSeries, baseline);
    mAreaSeries->setPen(Qt::NoPen);

    mChart = new QChart();
    mChart->addSeries(mSeries);
    mChart->addSeries(mAreaSeries);
    mChart->legend()->hide();
    mChart->setMargins(QMargins(0, 0, 0, 0));
    mChart->setBackgroundRoundness(0);
    mChart->layout()->setContentsMargins(0, 0, 0, 0);

    auto *axisX = new QValueAxis();
    axisX->setRange(0, SPARKLINE_SIZE - 1);
    axisX->setVisible(false);
    mChart->addAxis(axisX, Qt::AlignBottom);
    mSeries->attachAxis(axisX);
    mAreaSeries->attachAxis(axisX);

    auto *axisY = new QValueAxis();
    axisY->setRange(0, 100);
    axisY->setVisible(false);
    mChart->addAxis(axisY, Qt::AlignLeft);
    mSeries->attachAxis(axisY);
    mAreaSeries->attachAxis(axisY);

    mChartView = new QChartView(mChart, this);
    mChartView->setRenderHint(QPainter::Antialiasing);
    mChartView->setFixedHeight(30);
    mainLayout->addWidget(mChartView);

    mainLayout->addSpacing(2);

    createFooterLayout(mainLayout);

    // Initialize sparkline series from data buffer (pre-filled with zeros by base)
    mSeries->replace(mPointsCache);

    createGearButton();
}

void HybridTile::setValue(int percent, const QString &valueText)
{
    mPercent = qBound(0, percent, 100);
    mValueText = valueText;
    update();
}

void HybridTile::addDataPoint(double value)
{
    shiftDataPoint(value);

    updateSparkline();
    updateTrend();
    update();
}

void HybridTile::clearDataPoints()
{
    MetricTileBase::clearDataPoints();
    updateSparkline();
    updateTrend();
}

void HybridTile::setSubtitle(const QString &text)
{
    mLblSubtitle->setText(text);
}

void HybridTile::setTrendDirection(TrendDirection dir)
{
    mCurrentTrend = dir;
    mLblTrend->setText(trendText(dir));
}

void HybridTile::setSecondaryValue(const QString &text)
{
    mSecondaryText = text;
    update();
}

void HybridTile::setDisplayMode(DisplayMode mode)
{
    mDisplayMode = mode;

    switch (mode) {
    case Hero:
        mChartView->show();
        mGaugeArea->setMinimumHeight(100);
        mChartView->setFixedHeight(40);
        break;
    case Large:
        mChartView->show();
        mGaugeArea->setMinimumHeight(80);
        mChartView->setFixedHeight(35);
        break;
    case Compact:
        // Drop the sparkline entirely; the painted gauge + value carry the tile.
        mChartView->hide();
        mGaugeArea->setMinimumHeight(0);
        break;
    case Normal:
    default:
        mChartView->show();
        mGaugeArea->setMinimumHeight(0);
        mChartView->setFixedHeight(30);
        break;
    }
    update();
}

void HybridTile::setQuickAction(const QString &text, std::function<void()> callback)
{
    mBtnAction->setText(text);
    mBtnAction->show();
    mLblTrend->hide();
    QObject::disconnect(mBtnAction, &QPushButton::clicked, nullptr, nullptr);
    connect(mBtnAction, &QPushButton::clicked, this, [callback]() { callback(); });
}

void HybridTile::refreshThemeColors()
{
    QSettings *sv = AppManager::ins()->getStyleValues();
    if (!sv)
        return;

    mArcColor = resolvedColor();
    mTrackColor = QColor(sv->value("@color02").toString());
    mTextColor = QColor(sv->value("@color05").toString());
    mSecondaryTextColor = QColor(sv->value("@tertiaryText").toString());

    mSeries->setPen(QPen(mArcColor, 1.5));

    QColor fillColor = mArcColor;
    fillColor.setAlphaF(0.1);
    mAreaSeries->setBrush(fillColor);

    mChart->setBackgroundBrush(QColor(sv->value("@cardBg").toString()));

    applyActionButtonStyle(mArcColor, QColor(sv->value("@color07").toString()));

    updateGearIcon();
    update();
}

void HybridTile::paintEvent(QPaintEvent *event)
{
    QWidget::paintEvent(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    drawGaugeArc(painter);
}

void HybridTile::drawGaugeArc(QPainter &painter)
{
    QRect gaugeRect = mGaugeArea->geometry();
    if (gaugeRect.isEmpty())
        return;

    int side = gaugeSize();
    if (side < 20)
        return;

    int cx = gaugeRect.center().x();
    int cy = gaugeRect.center().y();
    QRect arcRect(cx - side / 2, cy - side / 2, side, side);

    // Track arc (background)
    QPen trackPen(mTrackColor, ARC_PEN_WIDTH, Qt::SolidLine, Qt::RoundCap);
    painter.setPen(trackPen);
    painter.drawArc(arcRect, GAUGE_START_ANGLE * 16, -GAUGE_SWEEP_ANGLE * 16);

    // Value arc
    double valueSweep = -(GAUGE_SWEEP_ANGLE * mPercent / 100.0);
    QPen valuePen(mArcColor, ARC_PEN_WIDTH, Qt::SolidLine, Qt::RoundCap);
    painter.setPen(valuePen);
    painter.drawArc(arcRect, GAUGE_START_ANGLE * 16, valueSweep * 16);

    // Percentage text centered in gauge
    QFont percentFont = painter.font();
    percentFont.setPixelSize(qMax(12, side / 4));
    percentFont.setBold(true);
    painter.setFont(percentFont);

    QString displayText = mValueText.isEmpty() ? QString("%1%").arg(mPercent) : mValueText;
    QRectF innerRect = arcRect.adjusted(ARC_PEN_WIDTH, ARC_PEN_WIDTH,
                                        -ARC_PEN_WIDTH, -ARC_PEN_WIDTH);

    if (!mSecondaryText.isEmpty()) {
        QFontMetrics pctFm(percentFont);
        int pctH = pctFm.height();

        QFont secondaryFont = painter.font();
        int secSize = qMin(qMax(9, side / 7), 13);
        secondaryFont.setPixelSize(secSize);
        secondaryFont.setBold(false);
        QFontMetrics secFm(secondaryFont);
        int secH = secFm.height();

        int gap = 2;
        int totalH = pctH + gap + secH;
        double textTop = cy - totalH / 2.0;

        QRectF pctRect(innerRect.left(), textTop, innerRect.width(), pctH);
        painter.setPen(mTextColor);
        painter.drawText(pctRect, Qt::AlignHCenter | Qt::AlignVCenter, displayText);

        QString elidedSec = secFm.elidedText(mSecondaryText, Qt::ElideRight,
                                              static_cast<int>(innerRect.width()));
        QRectF secRect(innerRect.left(), textTop + pctH + gap, innerRect.width(), secH);
        painter.setFont(secondaryFont);
        painter.setPen(mSecondaryTextColor);
        painter.drawText(secRect, Qt::AlignHCenter | Qt::AlignVCenter, elidedSec);
    } else {
        painter.setPen(mTextColor);
        painter.drawText(innerRect, Qt::AlignCenter, displayText);
    }
}

int HybridTile::gaugeSize() const
{
    QRect gaugeRect = mGaugeArea->geometry();
    int available = qMin(gaugeRect.width(), gaugeRect.height()) - ARC_PEN_WIDTH * 2;
    return qMax(0, available);
}

void HybridTile::updateSparkline()
{
    mSeries->replace(mPointsCache);
}

void HybridTile::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    repositionGearButton();
}
