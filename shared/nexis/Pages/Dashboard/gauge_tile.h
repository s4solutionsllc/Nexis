#ifndef GAUGE_TILE_H
#define GAUGE_TILE_H

#include "metric_tile_base.h"

#include <QLabel>
#include <QColor>

class GaugeTile : public MetricTileBase
{
    Q_OBJECT

public:
    explicit GaugeTile(const QString &title, const QString &colorToken, QWidget *parent = nullptr);
    ~GaugeTile() = default;

    // MetricTileBase overrides
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

    QColor mArcColor;
    QColor mArcEndColor;
    QColor mTrackColor;
    QColor mTextColor;
    QColor mSecondaryTextColor;

    int mPercent;
    QString mValueText;
    QString mSecondaryText;
};

#endif // GAUGE_TILE_H
