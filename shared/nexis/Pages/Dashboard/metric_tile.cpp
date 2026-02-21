#include "metric_tile.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include "Managers/app_manager.h"
#include "signal_mapper.h"

MetricTile::MetricTile(const QString &title, const QString &colorToken, QWidget *parent)
    : QWidget(parent),
      mTitle(title),
      mColorToken(colorToken),
      mDisplayMode(Normal),
      mCurrentTrend(Stable)
{
    setObjectName("metricTile");
    buildLayout();
    refreshThemeColors();

    connect(SignalMapper::ins(), &SignalMapper::sigChangedAppTheme, this, &MetricTile::refreshThemeColors);
}

void MetricTile::buildLayout()
{
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(12, 10, 12, 8);
    mainLayout->setSpacing(4);

    // Title
    mLblTitle = new QLabel(mTitle, this);
    mLblTitle->setObjectName("metricTileTitle");
    mainLayout->addWidget(mLblTitle);

    // Value + Secondary value row
    auto *valueLayout = new QHBoxLayout();
    valueLayout->setContentsMargins(0, 0, 0, 0);
    valueLayout->setSpacing(8);

    mLblValue = new QLabel("--", this);
    mLblValue->setObjectName("metricTileValue");
    mLblValue->setAlignment(Qt::AlignLeft | Qt::AlignBaseline);

    mLblSecondaryValue = new QLabel(this);
    mLblSecondaryValue->setObjectName("metricTileSubtitle");
    mLblSecondaryValue->setAlignment(Qt::AlignLeft | Qt::AlignBaseline);
    mLblSecondaryValue->hide();

    valueLayout->addWidget(mLblValue);
    valueLayout->addWidget(mLblSecondaryValue);
    valueLayout->addStretch();
    mainLayout->addLayout(valueLayout);

    // Sparkline chart
    mSeries = new QLineSeries();

    QLineSeries *baseline = new QLineSeries();
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
    mChartView->setMinimumHeight(40);
    mChartView->setMaximumHeight(60);
    mainLayout->addWidget(mChartView);

    // Progress bar (below sparkline per mockup)
    mProgressBar = new QProgressBar(this);
    mProgressBar->setObjectName("metricTileProgress");
    mProgressBar->setRange(0, 100);
    mProgressBar->setValue(0);
    mProgressBar->setTextVisible(false);
    mProgressBar->setFixedHeight(4);
    mainLayout->addWidget(mProgressBar);

    mainLayout->addSpacing(2);

    // Footer row: subtitle + trend + action
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

    // Initialize sparkline with empty data
    for (int i = 0; i < SPARKLINE_SIZE; ++i) {
        mDataBuffer.append(0.0);
        mSeries->append(i, 0);
    }
}

void MetricTile::setValue(int percent, const QString &valueText)
{
    mProgressBar->setValue(qBound(0, percent, 100));
    mLblValue->setText(valueText);
}

void MetricTile::addDataPoint(double value)
{
    if (mDataBuffer.size() >= SPARKLINE_SIZE)
        mDataBuffer.removeFirst();
    mDataBuffer.append(value);

    updateSparkline();
    updateTrend();
}

void MetricTile::setSubtitle(const QString &text)
{
    mLblSubtitle->setText(text);
}

void MetricTile::setTrendDirection(TrendDirection dir)
{
    mCurrentTrend = dir;
    switch (dir) {
    case Rising:
        mLblTrend->setText(QStringLiteral("\u2191 rising"));
        break;
    case Falling:
        mLblTrend->setText(QStringLiteral("\u2193 falling"));
        break;
    case Stable:
        mLblTrend->setText(QStringLiteral("\u2192 stable"));
        break;
    }
}

void MetricTile::setQuickAction(const QString &text, std::function<void()> callback)
{
    mBtnAction->setText(text);
    mBtnAction->show();
    mLblTrend->hide();
    QObject::disconnect(mBtnAction, &QPushButton::clicked, nullptr, nullptr);
    connect(mBtnAction, &QPushButton::clicked, this, [callback]() { callback(); });
}

void MetricTile::setDisplayMode(DisplayMode mode)
{
    mDisplayMode = mode;

    mLblValue->setProperty("heroMode", mode == Hero ? "true" : "false");
    mLblValue->setProperty("largeMode", mode == Large ? "true" : "false");

    mLblValue->style()->unpolish(mLblValue);
    mLblValue->style()->polish(mLblValue);
}

void MetricTile::setSecondaryValue(const QString &text)
{
    mLblSecondaryValue->setText(text);
    mLblSecondaryValue->setVisible(!text.isEmpty());
}

void MetricTile::refreshThemeColors()
{
    QSettings *sv = AppManager::ins()->getStyleValues();
    if (!sv)
        return;

    QColor color(sv->value(mColorToken).toString());
    QString colorHex = color.name();
    QString hoverText = sv->value("@color07").toString();

    mSeries->setPen(QPen(color, 1.5));

    QColor fillColor = color;
    fillColor.setAlphaF(0.1);
    mAreaSeries->setBrush(fillColor);

    mChart->setBackgroundBrush(QColor(sv->value("@cardBg").toString()));

    mProgressBar->setStyleSheet(
        QString("QProgressBar#metricTileProgress::chunk { background-color: %1; border-radius: 2; }").arg(colorHex));

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
}

void MetricTile::updateSparkline()
{
    mSeries->clear();
    for (int i = 0; i < mDataBuffer.size(); ++i)
        mSeries->append(i, mDataBuffer.at(i));
}

void MetricTile::updateTrend()
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
