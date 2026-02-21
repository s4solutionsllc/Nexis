#ifndef METRIC_TILE_H
#define METRIC_TILE_H

#include <QWidget>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QColor>
#include <QtCharts>
#include <functional>

class MetricTile : public QWidget
{
    Q_OBJECT

public:
    enum TrendDirection { Rising, Falling, Stable };
    enum DisplayMode { Normal, Hero, Large };

    explicit MetricTile(const QString &title, const QColor &color, QWidget *parent = nullptr);
    ~MetricTile() = default;

    void setValue(int percent, const QString &valueText);
    void addDataPoint(double value);
    void setSubtitle(const QString &text);
    void setTrendDirection(TrendDirection dir);
    void setQuickAction(const QString &text, std::function<void()> callback);
    void setDisplayMode(DisplayMode mode);
    void setSecondaryValue(const QString &text);

private:
    void buildLayout();
    void updateSparkline();
    void updateTrend();

    QString mTitle;
    QColor mColor;
    DisplayMode mDisplayMode;

    QLabel *mLblTitle;
    QLabel *mLblValue;
    QLabel *mLblSecondaryValue;
    QProgressBar *mProgressBar;
    QChartView *mChartView;
    QLineSeries *mSeries;
    QAreaSeries *mAreaSeries;
    QChart *mChart;
    QLabel *mLblSubtitle;
    QLabel *mLblTrend;
    QPushButton *mBtnAction;

    static const int SPARKLINE_SIZE = 60;
    QList<double> mDataBuffer;

    TrendDirection mCurrentTrend;
};

#endif // METRIC_TILE_H
