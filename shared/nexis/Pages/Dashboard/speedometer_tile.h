#ifndef SPEEDOMETER_TILE_H
#define SPEEDOMETER_TILE_H

#include "metric_tile_base.h"

#include <QLabel>
#include <QColor>

class SpeedometerTile : public MetricTileBase
{
    Q_OBJECT

public:
    explicit SpeedometerTile(const QString &title, const QString &colorToken, QWidget *parent = nullptr);
    ~SpeedometerTile() = default;

    void setValue(int percent, const QString &valueText) override;
    void addDataPoint(double value) override;
    void setSubtitle(const QString &text) override;
    void setTrendDirection(TrendDirection dir) override;
    void setSecondaryValue(const QString &text) override;
    void setDisplayMode(DisplayMode mode) override;
    void setQuickAction(const QString &text, std::function<void()> callback) override;
    void refreshThemeColors() override;

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void buildLayout();
    QColor gradientColorAt(double fraction) const;

    int mPercent;
    QString mValueText;
    QString mSecondaryText;

    QColor mMetricColor;
    QColor mCardBgColor;
    QColor mTextColor;
    QColor mSecondaryTextColor;
    QColor mGreenColor;
    QColor mYellowColor;
    QColor mOrangeColor;
    QColor mRedColor;
};

#endif // SPEEDOMETER_TILE_H
