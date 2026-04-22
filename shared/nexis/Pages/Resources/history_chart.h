#ifndef HISTORYCHART_H
#define HISTORYCHART_H

#include <QWidget>
#include <QDebug>
#include <QtCharts>
#include <QTimer>

#include "Utils/format_util.h"

class AppManager;
class SignalMapper;

namespace Ui {
    class HistoryChart;
}

class HistoryChart : public QWidget
{
    Q_OBJECT

public:
    explicit HistoryChart(const QString &title, const int &seriesCount,
                          QCategoryAxis* categoriAxisY = nullptr, QWidget *parent = 0,
                          AppManager *appManager = nullptr, SignalMapper *signalMapper = nullptr);
    ~HistoryChart();

    QVector<QSplineSeries *> getSeriesList() const;
    QCategoryAxis *getAxisY();
    void setYMax(double value);
    void setSeriesList(const QVector<QSplineSeries *> &seriesList);
    void setCategoryAxisYLabels();

private slots:
    void on_checkHistoryTitle_clicked(bool checked);

private:
    void init();

private:
    Ui::HistoryChart *ui;

    AppManager *mAppManager;
    SignalMapper *mSignalMapper;

    QString mTitle;
    int mSeriesCount;
    QChartView *mChartView;
    QChart *mChart;
    QVector<QSplineSeries *> mSeriesList;

    QCategoryAxis *mAxisY;
};

#endif // HISTORYCHART_H
