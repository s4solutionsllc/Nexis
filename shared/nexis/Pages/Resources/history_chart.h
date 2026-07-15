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

    // DS §2/§3 (NEX-Resources): opt a chart into the elevated-card header
    // treatment — reveals + colors the accent bar (accentToken is one of the
    // #sectionHeaderAccent[accentToken=...] values in style.qss, e.g. "cpu",
    // "gpu", "disk"), pads the header/body, switches the plot surface to the
    // @chartBackgroundColor token, and applies the DS §2 drop shadow. Charts
    // that don't call this keep today's flat, unshadowed-by-default chrome.
    void setElevated(const QString &accentToken);

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

    QString mBackgroundToken = QStringLiteral("@historyChartBackgroundColor");
};

#endif // HISTORYCHART_H
