#include "history_chart.h"
#include "ui_history_chart.h"
#include "dpi.h"

HistoryChart::~HistoryChart()
{
    delete ui;
}

HistoryChart::HistoryChart(const QString &title, const int &seriesCount, QCategoryAxis* categoriAxisY, QWidget *parent) :
    QWidget(parent),
    ui(new Ui::HistoryChart),
    mTitle(title),
    mSeriesCount(seriesCount),
    mChartView(new QChartView(this)),
    mChart(mChartView->chart())
{
    ui->setupUi(this);

    init();

    if (categoriAxisY) {
        mAxisY = categoriAxisY;
        mAxisY->setLabelsPosition(QCategoryAxis::AxisLabelsPositionOnValue);
        // Remove the default Y axis created by createDefaultAxes() before adding the custom one
        QList<QAbstractAxis*> verticalAxes = mChart->axes(Qt::Vertical);
        for (QAbstractAxis *axis : verticalAxes) {
            mChart->removeAxis(axis);
        }
        mChart->addAxis(mAxisY, Qt::AlignLeft);
        for (int i = 0; i < seriesCount; ++i) {
            mSeriesList.at(i)->attachAxis(mAxisY);
        }
    }
}

void HistoryChart::init()
{
    ui->lblHistoryTitle->setText(mTitle);

    // add series to chart
    for (int i = 0; i < mSeriesCount; i++) {
        mSeriesList.append(new QSplineSeries);
        mChart->addSeries(mSeriesList.at(i));
    }

    mChartView->setRenderHint(QPainter::Antialiasing);

    QList<int> colors = {
        0x2ec27e, 0xe01b24, 0x1c71d8, 0xe5a50a, 0xE95420,
        0x26a269, 0x813d9c, 0x241f31, 0xc64516, 0xc01c28,
        0x613583, 0xcd9309, 0xa51d2d, 0x3d3846, 0x77767b,
        0x1a5fb4, 0x33d17a, 0xf66151, 0xf8e45c, 0x5e5c64
    };
    // set colors
    for (int i = 0; i < mSeriesList.count(); ++i) {
        dynamic_cast<QSplineSeries*>(mChart->series().at(i))->setColor(QColor(static_cast<QRgb>(colors.at(i))));
    }

    // Chart Settings
    mChart->createDefaultAxes();

    mChart->axes(Qt::Horizontal).first()->setRange(0, 60);
    mChart->axes(Qt::Horizontal).first()->setReverse(true);

    mChart->setContentsMargins(-Dpi::scale(11), -Dpi::scale(11), -Dpi::scale(11), -Dpi::scale(11));
    mChart->setMargins(QMargins(Dpi::scale(20), 0, Dpi::scale(10), Dpi::scale(10)));
    ui->layoutHistoryChart->addWidget(mChartView, 1, 0, 1, 3);

    // theme changed
    connect(SignalMapper::ins(), &SignalMapper::sigChangedAppTheme, [=] {
        QString chartLabelColor = AppManager::ins()->getStyleValues()->value("@chartLabelColor").toString();
        QString chartGridColor = AppManager::ins()->getStyleValues()->value("@chartGridColor").toString();
        QString historyChartBackground = AppManager::ins()->getStyleValues()->value("@historyChartBackgroundColor").toString();

        mChart->axes(Qt::Horizontal).first()->setLabelsColor(chartLabelColor);
        mChart->axes(Qt::Horizontal).first()->setGridLineColor(chartGridColor);

        mChart->axes(Qt::Vertical).first()->setLabelsColor(chartLabelColor);
        mChart->axes(Qt::Vertical).first()->setGridLineColor(chartGridColor);

        mChart->setBackgroundBrush(QColor(historyChartBackground));
        mChart->legend()->setLabelColor(chartLabelColor);
    });
}

void HistoryChart::setYMax(const int &value)
{
    mChart->axes(Qt::Vertical).first()->setRange(0, value);
}

QCategoryAxis *HistoryChart::getAxisY()
{
    return mAxisY;
}

void HistoryChart::setCategoryAxisYLabels()
{
    if (mAxisY) {
        for (const QString &label : mAxisY->categoriesLabels()){
            mAxisY->remove(label);
        }

        for (int i = 1; i < 5; ++i) {
            mAxisY->append(FormatUtil::formatBytes((mAxisY->max()/4)*i), (mAxisY->max()/4)*i);
        }
    }
}

QVector<QSplineSeries*> HistoryChart::getSeriesList() const
{
    return mSeriesList;
}

void HistoryChart::setSeriesList(const QVector<QSplineSeries *> &seriesList)
{
    Q_UNUSED(seriesList);
    // In Qt6, series() returns by value so modifying the returned list is a no-op.
    // The series are already being modified in-place via their pointers (insert, replace, removePoints),
    // so we only need to trigger a repaint.
    mChartView->repaint();
}

void HistoryChart::on_checkHistoryTitle_clicked(bool checked)
{
    QLayout *charts = topLevelWidget()->findChild<QWidget*>("charts")->layout();

    for (int i = 0; i < charts->count(); ++i) {
        charts->itemAt(i)->widget()->setVisible(! checked);
    }

    show();
}
