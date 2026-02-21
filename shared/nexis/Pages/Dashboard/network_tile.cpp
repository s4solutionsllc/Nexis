#include "network_tile.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include "Managers/app_manager.h"
#include "signal_mapper.h"

NetworkTile::NetworkTile(const QColor &color, QWidget *parent)
    : QWidget(parent),
      mColor(color),
      mMaxSeen(1024.0 * 1024.0)
{
    setObjectName("metricTile");
    buildLayout();

    connect(SignalMapper::ins(), &SignalMapper::sigChangedAppTheme, this, [this]() {
        QSettings *sv = AppManager::ins()->getStyleValues();
        mChart->setBackgroundBrush(QColor(sv->value("@cardBg").toString()));
    });
}

void NetworkTile::buildLayout()
{
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(12, 10, 12, 8);
    mainLayout->setSpacing(4);

    // Title
    mLblTitle = new QLabel(tr("NETWORK"), this);
    mLblTitle->setObjectName("metricTileTitle");
    mainLayout->addWidget(mLblTitle);

    // Download / Upload values
    auto *valLayout = new QHBoxLayout();
    valLayout->setContentsMargins(0, 0, 0, 0);

    mLblDownValue = new QLabel(QStringLiteral("\u2193 0 B/s"), this);
    mLblDownValue->setObjectName("metricTileValue");
    mLblDownValue->setStyleSheet("font-size: 13pt;");

    mLblUpValue = new QLabel(QStringLiteral("\u2191 0 B/s"), this);
    mLblUpValue->setObjectName("metricTileSubtitle");
    mLblUpValue->setStyleSheet("font-size: 10pt;");
    mLblUpValue->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    valLayout->addWidget(mLblDownValue);
    valLayout->addStretch();
    valLayout->addWidget(mLblUpValue);
    mainLayout->addLayout(valLayout);

    // Dual sparkline chart
    mRxSeries = new QLineSeries();
    mRxSeries->setPen(QPen(mColor, 1.5));

    QColor txColor = mColor.lighter(140);
    mTxSeries = new QLineSeries();
    QPen txPen(txColor, 1.0, Qt::DashLine);
    mTxSeries->setPen(txPen);

    mChart = new QChart();
    mChart->addSeries(mRxSeries);
    mChart->addSeries(mTxSeries);
    mChart->legend()->hide();
    mChart->setMargins(QMargins(0, 0, 0, 0));
    mChart->setBackgroundRoundness(0);
    mChart->layout()->setContentsMargins(0, 0, 0, 0);

    QSettings *sv = AppManager::ins()->getStyleValues();
    if (sv)
        mChart->setBackgroundBrush(QColor(sv->value("@cardBg").toString()));
    else
        mChart->setBackgroundBrush(Qt::transparent);

    auto *axisX = new QValueAxis();
    axisX->setRange(0, SPARKLINE_SIZE - 1);
    axisX->setVisible(false);
    mChart->addAxis(axisX, Qt::AlignBottom);
    mRxSeries->attachAxis(axisX);
    mTxSeries->attachAxis(axisX);

    mAxisY = new QValueAxis();
    mAxisY->setRange(0, mMaxSeen);
    mAxisY->setVisible(false);
    mChart->addAxis(mAxisY, Qt::AlignLeft);
    mRxSeries->attachAxis(mAxisY);
    mTxSeries->attachAxis(mAxisY);

    mChartView = new QChartView(mChart, this);
    mChartView->setRenderHint(QPainter::Antialiasing);
    mChartView->setMinimumHeight(40);
    mChartView->setMaximumHeight(60);
    mainLayout->addWidget(mChartView);

    // Footer
    auto *footerLayout = new QHBoxLayout();
    footerLayout->setContentsMargins(0, 0, 0, 0);

    mLblFooter = new QLabel(this);
    mLblFooter->setObjectName("metricTileSubtitle");

    mBtnAction = new QPushButton(this);
    mBtnAction->setObjectName("metricTileAction");
    mBtnAction->setCursor(Qt::PointingHandCursor);
    mBtnAction->setFocusPolicy(Qt::NoFocus);
    mBtnAction->hide();
    mBtnAction->setFixedHeight(22);
    mBtnAction->setStyleSheet(
        "QPushButton#metricTileAction {"
        "  font-size: 8pt;"
        "  padding: 2px 8px;"
        "  border-radius: 10px;"
        "  border: 1px solid " + mColor.name() + ";"
        "  color: " + mColor.name() + ";"
        "  background: transparent;"
        "}"
        "QPushButton#metricTileAction:hover {"
        "  background-color: " + mColor.name() + ";"
        "  color: #ffffff;"
        "}"
    );

    footerLayout->addWidget(mLblFooter);
    footerLayout->addStretch();
    footerLayout->addWidget(mBtnAction);
    mainLayout->addLayout(footerLayout);

    // Initialize buffers
    for (int i = 0; i < SPARKLINE_SIZE; ++i) {
        mRxBuffer.append(0.0);
        mTxBuffer.append(0.0);
        mRxSeries->append(i, 0);
        mTxSeries->append(i, 0);
    }
}

void NetworkTile::setValues(quint64 rxDelta, quint64 txDelta, quint64 rxTotal, quint64 txTotal)
{
    mLblDownValue->setText(QStringLiteral("\u2193 %1/s").arg(FormatUtil::formatBytes(rxDelta)));
    mLblUpValue->setText(QStringLiteral("\u2191 %1/s").arg(FormatUtil::formatBytes(txDelta)));
    mLblFooter->setText(tr("Total: \u2193%1  \u2191%2")
                            .arg(FormatUtil::formatBytes(rxTotal),
                                 FormatUtil::formatBytes(txTotal)));

    // Update sparkline buffers
    if (mRxBuffer.size() >= SPARKLINE_SIZE) mRxBuffer.removeFirst();
    if (mTxBuffer.size() >= SPARKLINE_SIZE) mTxBuffer.removeFirst();
    mRxBuffer.append(static_cast<double>(rxDelta));
    mTxBuffer.append(static_cast<double>(txDelta));

    double localMax = 1024.0;
    for (double v : mRxBuffer) localMax = qMax(localMax, v);
    for (double v : mTxBuffer) localMax = qMax(localMax, v);
    mMaxSeen = localMax * 1.1;
    mAxisY->setRange(0, mMaxSeen);

    mRxSeries->clear();
    mTxSeries->clear();
    for (int i = 0; i < mRxBuffer.size(); ++i) {
        mRxSeries->append(i, mRxBuffer.at(i));
        mTxSeries->append(i, mTxBuffer.at(i));
    }
}

void NetworkTile::setQuickAction(const QString &text, std::function<void()> callback)
{
    mBtnAction->setText(text);
    mBtnAction->show();
    QObject::disconnect(mBtnAction, &QPushButton::clicked, nullptr, nullptr);
    connect(mBtnAction, &QPushButton::clicked, this, [callback]() { callback(); });
}
