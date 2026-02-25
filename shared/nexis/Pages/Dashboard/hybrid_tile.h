#ifndef HYBRID_TILE_H
#define HYBRID_TILE_H

#include "metric_tile_base.h"

#include <QLabel>
#include <QColor>
#include <QPushButton>
#include <QtCharts>

class HybridTile : public MetricTileBase
{
    Q_OBJECT

public:
    explicit HybridTile(const QString &title, const QString &colorToken, QWidget *parent = nullptr);
    ~HybridTile() = default;

    // MetricTileBase overrides
    void setValue(int percent, const QString &valueText) override;
    void addDataPoint(double value) override;
    void clearDataPoints() override;
    void setSubtitle(const QString &text) override;
    void setTrendDirection(TrendDirection dir) override;
    void setSecondaryValue(const QString &text) override;
    void setDisplayMode(DisplayMode mode) override;
    void setQuickAction(const QString &text, std::function<void()> callback) override;
    QToolButton *gearButton() override;
    void setGearVisible(bool visible) override;
    void refreshThemeColors() override;

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void buildLayout();
    void updateGearIcon();
    void updateSparkline();
    void updateTrend();
    void drawGaugeArc(QPainter &painter);
    int gaugeSize() const;

    QColor mArcColor;
    QColor mTrackColor;
    QColor mTextColor;
    QColor mSecondaryTextColor;

    int mPercent;
    QString mValueText;
    QString mSecondaryText;

    QLabel *mLblTitle;
    QWidget *mGaugeArea;
    QChartView *mChartView;
    QLineSeries *mSeries;
    QAreaSeries *mAreaSeries;
    QChart *mChart;
    QLabel *mLblSubtitle;
    QLabel *mLblTrend;
    QPushButton *mBtnAction;
    QToolButton *mGearButton;

    TrendDirection mCurrentTrend;
};

#endif // HYBRID_TILE_H
