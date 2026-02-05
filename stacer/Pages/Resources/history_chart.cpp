#include "history_chart.h"
#include "ui_history_chart.h"

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
        0x2ecc71, 0xe74c3c, 0x3498db, 0xf1c40f, 0xe67e22,
        0x1abc9c, 0x9b59b6, 0x34495e, 0xd35400, 0xc0392b,
        0x8e44ad, 0xFF8F00, 0xEF6C00, 0x4E342E, 0x424242,
        0x5499C7, 0x58D68D, 0xCD6155, 0xF5B041, 0x566573
    };
    // set colors
    for (int i = 0; i < mSeriesList.count(); ++i) {
        dynamic_cast<QSplineSeries*>(mChart->series().at(i))->setColor(QColor(static_cast<QRgb>(colors.at(i))));
    }

    // Chart Settings
    mChart->createDefaultAxes();

    mChart->axes(Qt::Horizontal).first()->setRange(0, 60);
    mChart->axes(Qt::Horizontal).first()->setReverse(true);

    mChart->setContentsMargins(-11, -11, -11, -11);
    mChart->setMargins(QMargins(20, 0, 10, 10));
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
