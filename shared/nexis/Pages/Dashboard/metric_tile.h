#ifndef METRIC_TILE_H
#define METRIC_TILE_H

#include "metric_tile_base.h"

#include <QLabel>
#include <QProgressBar>
#include <QtCharts>

class MetricTile : public MetricTileBase
{
    Q_OBJECT

public:
    explicit MetricTile(const QString &title, const QString &colorToken, QWidget *parent = nullptr);
    ~MetricTile() = default;

    void setValue(int percent, const QString &valueText) override;
    void addDataPoint(double value) override;
    void clearDataPoints() override;
    void setSubtitle(const QString &text) override;
    void setTrendDirection(TrendDirection dir) override;
    void setQuickAction(const QString &text, std::function<void()> callback) override;
    void setDisplayMode(DisplayMode mode) override;
    void setSecondaryValue(const QString &text) override;

    void refreshThemeColors() override;

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    void buildLayout();
    void updateSparkline();

    QProgressBar *mProgressBar;
    QChartView *mChartView;
    QLineSeries *mSeries;
    QAreaSeries *mAreaSeries;
    QChart *mChart;
};

#endif // METRIC_TILE_H
