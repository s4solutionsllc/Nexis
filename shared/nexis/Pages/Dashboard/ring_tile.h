#ifndef RING_TILE_H
#define RING_TILE_H

#include "metric_tile_base.h"

#include <QLabel>
#include <QProgressBar>

class RingTile : public MetricTileBase
{
    Q_OBJECT

public:
    explicit RingTile(const QString &title, const QString &colorToken, QWidget *parent = nullptr);
    ~RingTile() = default;

    void setValue(int percent, const QString &valueText) override;
    void addDataPoint(double value) override;
    void setSubtitle(const QString &text) override;
    void setTrendDirection(TrendDirection dir) override;
    void setQuickAction(const QString &text, std::function<void()> callback) override;
    void setDisplayMode(DisplayMode mode) override;
    void setSecondaryValue(const QString &text) override;

    void refreshThemeColors() override;

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void buildLayout();
    int ringThickness() const;
    int ringFontSize() const;
    int secondaryFontSize() const;

    QLabel *mLblTitle;
    QLabel *mLblPercentage;
    QLabel *mLblSecondaryValue;
    QProgressBar *mProgressBar;

    int mPercent;

    QColor mMetricColor;
    QColor mTrackColor;
    QColor mSecondaryTextColor;
};

#endif // RING_TILE_H
