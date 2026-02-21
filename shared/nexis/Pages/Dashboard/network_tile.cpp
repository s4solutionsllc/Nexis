#include "network_tile.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include "Managers/app_manager.h"
#include "signal_mapper.h"

NetworkTile::NetworkTile(const QColor &color, QWidget *parent)
    : QWidget(parent),
      mColor(color),
      mUploadColor("#E05454"),
      mMaxSeen(1024.0 * 1024.0)
{
    setObjectName("networkTile");
    buildLayout();

    connect(SignalMapper::ins(), &SignalMapper::sigChangedAppTheme, this, [this]() {
        QSettings *sv = AppManager::ins()->getStyleValues();
        QString cardBg = sv->value("@cardBg").toString();
        mRxChart->setBackgroundBrush(QColor(cardBg));
        mTxChart->setBackgroundBrush(QColor(cardBg));
    });
}

void NetworkTile::buildLayout()
{
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(12, 10, 12, 8);
    mainLayout->setSpacing(4);

    // Title
    mLblTitle = new QLabel(tr("NETWORK"), this);
    mLblTitle->setObjectName("networkTileTitle");
    mainLayout->addWidget(mLblTitle);

    mainLayout->addSpacing(2);

    // Download row: label + value
    auto *downRow = new QHBoxLayout();
    downRow->setContentsMargins(0, 0, 0, 0);

    mLblDownLabel = new QLabel(QStringLiteral("\u2193 Download"), this);
    mLblDownLabel->setObjectName("networkTileLabel");
    mLblDownLabel->setStyleSheet(QString("color: %1;").arg(mColor.name()));

    mLblDownValue = new QLabel("0 B/s", this);
    mLblDownValue->setObjectName("networkTileValue");
    mLblDownValue->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    downRow->addWidget(mLblDownLabel);
    downRow->addStretch();
    downRow->addWidget(mLblDownValue);
    mainLayout->addLayout(downRow);

    // Download sparkline
    mRxSeries = new QLineSeries();
    mRxSeries->setPen(QPen(mColor, 1.5));

    QLineSeries *rxBaseline = new QLineSeries();
    for (int i = 0; i < SPARKLINE_SIZE; ++i)
        rxBaseline->append(i, 0);

    mRxAreaSeries = new QAreaSeries(mRxSeries, rxBaseline);
    QColor rxFill = mColor;
    rxFill.setAlphaF(0.08);
    mRxAreaSeries->setBrush(rxFill);
    mRxAreaSeries->setPen(Qt::NoPen);

    mRxChart = new QChart();
    mRxChart->addSeries(mRxSeries);
    mRxChart->addSeries(mRxAreaSeries);
    mRxChart->legend()->hide();
    mRxChart->setMargins(QMargins(0, 0, 0, 0));
    mRxChart->setBackgroundRoundness(0);
    mRxChart->layout()->setContentsMargins(0, 0, 0, 0);

    QSettings *sv = AppManager::ins()->getStyleValues();
    if (sv)
        mRxChart->setBackgroundBrush(QColor(sv->value("@cardBg").toString()));
    else
        mRxChart->setBackgroundBrush(Qt::transparent);

    auto *rxAxisX = new QValueAxis();
    rxAxisX->setRange(0, SPARKLINE_SIZE - 1);
    rxAxisX->setVisible(false);
    mRxChart->addAxis(rxAxisX, Qt::AlignBottom);
    mRxSeries->attachAxis(rxAxisX);
    mRxAreaSeries->attachAxis(rxAxisX);

    mRxAxisY = new QValueAxis();
    mRxAxisY->setRange(0, mMaxSeen);
    mRxAxisY->setVisible(false);
    mRxChart->addAxis(mRxAxisY, Qt::AlignLeft);
    mRxSeries->attachAxis(mRxAxisY);
    mRxAreaSeries->attachAxis(mRxAxisY);

    mRxChartView = new QChartView(mRxChart, this);
    mRxChartView->setRenderHint(QPainter::Antialiasing);
    mRxChartView->setMinimumHeight(24);
    mRxChartView->setMaximumHeight(36);
    mainLayout->addWidget(mRxChartView);

    mainLayout->addSpacing(2);

    // Upload row: label + value
    auto *upRow = new QHBoxLayout();
    upRow->setContentsMargins(0, 0, 0, 0);

    mLblUpLabel = new QLabel(QStringLiteral("\u2191 Upload"), this);
    mLblUpLabel->setObjectName("networkTileLabel");
    mLblUpLabel->setStyleSheet(QString("color: %1;").arg(mUploadColor.name()));

    mLblUpValue = new QLabel("0 B/s", this);
    mLblUpValue->setObjectName("networkTileValue");
    mLblUpValue->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    upRow->addWidget(mLblUpLabel);
    upRow->addStretch();
    upRow->addWidget(mLblUpValue);
    mainLayout->addLayout(upRow);

    // Upload sparkline
    mTxSeries = new QLineSeries();
    mTxSeries->setPen(QPen(mUploadColor, 1.5));

    QLineSeries *txBaseline = new QLineSeries();
    for (int i = 0; i < SPARKLINE_SIZE; ++i)
        txBaseline->append(i, 0);

    mTxAreaSeries = new QAreaSeries(mTxSeries, txBaseline);
    QColor txFill = mUploadColor;
    txFill.setAlphaF(0.08);
    mTxAreaSeries->setBrush(txFill);
    mTxAreaSeries->setPen(Qt::NoPen);

    mTxChart = new QChart();
    mTxChart->addSeries(mTxSeries);
    mTxChart->addSeries(mTxAreaSeries);
    mTxChart->legend()->hide();
    mTxChart->setMargins(QMargins(0, 0, 0, 0));
    mTxChart->setBackgroundRoundness(0);
    mTxChart->layout()->setContentsMargins(0, 0, 0, 0);

    if (sv)
        mTxChart->setBackgroundBrush(QColor(sv->value("@cardBg").toString()));
    else
        mTxChart->setBackgroundBrush(Qt::transparent);

    auto *txAxisX = new QValueAxis();
    txAxisX->setRange(0, SPARKLINE_SIZE - 1);
    txAxisX->setVisible(false);
    mTxChart->addAxis(txAxisX, Qt::AlignBottom);
    mTxSeries->attachAxis(txAxisX);
    mTxAreaSeries->attachAxis(txAxisX);

    mTxAxisY = new QValueAxis();
    mTxAxisY->setRange(0, mMaxSeen);
    mTxAxisY->setVisible(false);
    mTxChart->addAxis(mTxAxisY, Qt::AlignLeft);
    mTxSeries->attachAxis(mTxAxisY);
    mTxAreaSeries->attachAxis(mTxAxisY);

    mTxChartView = new QChartView(mTxChart, this);
    mTxChartView->setRenderHint(QPainter::Antialiasing);
    mTxChartView->setMinimumHeight(24);
    mTxChartView->setMaximumHeight(36);
    mainLayout->addWidget(mTxChartView);

    // Divider
    mDivider = new QFrame(this);
    mDivider->setObjectName("networkDivider");
    mDivider->setFrameShape(QFrame::HLine);
    mDivider->setFixedHeight(1);
    mainLayout->addWidget(mDivider);

    // Footer
    mLblFooter = new QLabel(this);
    mLblFooter->setObjectName("networkTileFooter");
    mainLayout->addWidget(mLblFooter);

    // Interface name
    mLblInterface = new QLabel(this);
    mLblInterface->setObjectName("networkTileInterface");
    mainLayout->addWidget(mLblInterface);

    // Action button (hidden by default)
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
    mainLayout->addWidget(mBtnAction);

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
    mLblDownValue->setText(QString("%1/s").arg(FormatUtil::formatBytes(rxDelta)));
    mLblUpValue->setText(QString("%1/s").arg(FormatUtil::formatBytes(txDelta)));
    mLblFooter->setText(tr("Total \u2193 %1  \u2022  Total \u2191 %2")
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
    mRxAxisY->setRange(0, mMaxSeen);
    mTxAxisY->setRange(0, mMaxSeen);

    mRxSeries->clear();
    mTxSeries->clear();
    for (int i = 0; i < mRxBuffer.size(); ++i) {
        mRxSeries->append(i, mRxBuffer.at(i));
        mTxSeries->append(i, mTxBuffer.at(i));
    }
}

void NetworkTile::setInterfaceName(const QString &name)
{
    mLblInterface->setText(tr("Interface: %1").arg(name));
}

void NetworkTile::setQuickAction(const QString &text, std::function<void()> callback)
{
    mBtnAction->setText(text);
    mBtnAction->show();
    QObject::disconnect(mBtnAction, &QPushButton::clicked, nullptr, nullptr);
    connect(mBtnAction, &QPushButton::clicked, this, [callback]() { callback(); });
}
