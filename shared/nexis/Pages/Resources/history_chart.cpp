#include "history_chart.h"
#include "ui_history_chart.h"
#include "dpi.h"
#include "Managers/app_manager.h"
#include "signal_mapper.h"

HistoryChart::~HistoryChart()
{
    delete ui;
}

HistoryChart::HistoryChart(const QString &title, const int &seriesCount,
                           QCategoryAxis* categoriAxisY, QWidget *parent,
                           AppManager *appManager, SignalMapper *signalMapper) :
    QWidget(parent),
    ui(new Ui::HistoryChart),
    mAppManager(appManager ? appManager : AppManager::ins()),
    mSignalMapper(signalMapper ? signalMapper : SignalMapper::ins()),
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

    QSettings *sv = mAppManager->getStyleValues();
    if (sv) {
        for (int i = 0; i < mSeriesList.count(); ++i) {
            QString token = QString("@chartSeries%1").arg(i + 1, 2, 10, QChar('0'));
            QColor c(sv->value(token).toString());
            dynamic_cast<QSplineSeries*>(mChart->series().at(i))->setColor(c);
        }
    }

    // Chart Settings
    mChart->createDefaultAxes();

    mChart->axes(Qt::Horizontal).first()->setRange(0, 60);
    mChart->axes(Qt::Horizontal).first()->setReverse(true);

    mChart->setContentsMargins(-Dpi::scale(11), -Dpi::scale(11), -Dpi::scale(11), -Dpi::scale(11));
    mChart->setMargins(QMargins(Dpi::scale(20), 0, Dpi::scale(10), Dpi::scale(10)));
    ui->layoutHistoryChart->addWidget(mChartView, 1, 0, 1, 3);

    connect(mSignalMapper, &SignalMapper::sigChangedAppTheme, [=] {
        QSettings *sv = mAppManager->getStyleValues();
        if (!sv) return;
        QString chartLabelColor = sv->value("@chartLabelColor").toString();
        QString chartGridColor = sv->value("@chartGridColor").toString();
        QString historyChartBackground = sv->value("@historyChartBackgroundColor").toString();

        mChart->axes(Qt::Horizontal).first()->setLabelsColor(chartLabelColor);
        mChart->axes(Qt::Horizontal).first()->setGridLineColor(chartGridColor);

        mChart->axes(Qt::Vertical).first()->setLabelsColor(chartLabelColor);
        mChart->axes(Qt::Vertical).first()->setGridLineColor(chartGridColor);

        mChart->setBackgroundBrush(QColor(historyChartBackground));
        mChart->legend()->setLabelColor(chartLabelColor);

        for (int i = 0; i < mSeriesList.count(); ++i) {
            QString token = QString("@chartSeries%1").arg(i + 1, 2, 10, QChar('0'));
            QColor c(sv->value(token).toString());
            dynamic_cast<QSplineSeries*>(mChart->series().at(i))->setColor(c);
        }
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
