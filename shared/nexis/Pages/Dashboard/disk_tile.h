#ifndef DISK_TILE_H
#define DISK_TILE_H

#include "metric_tile_base.h"

#include <QColor>

class DiskTile : public MetricTileBase
{
    Q_OBJECT

public:
    explicit DiskTile(const QString &arcColorToken, const QString &trackColorToken, QWidget *parent = nullptr);
    ~DiskTile() = default;

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

    QString mTrackColorToken;
    QColor mArcColor;
    QColor mTrackColor;
    QColor mTextColor;

    int mPercent;
    QString mValueText;
};

#endif // DISK_TILE_H
