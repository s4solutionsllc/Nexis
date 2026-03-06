#ifndef VUMETER_TILE_H
#define VUMETER_TILE_H

#include "metric_tile_base.h"

#include <QLabel>
#include <QColor>

class VuMeterTile : public MetricTileBase
{
    Q_OBJECT

public:
    explicit VuMeterTile(const QString &title, const QString &colorToken, QWidget *parent = nullptr);
    ~VuMeterTile() = default;

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

    int segmentCount() const;
    int barWidth() const;
    int scaleFontSize() const;

    QColor segmentColor(int segmentIndex, int totalSegments) const;

    int mPercent;
    QString mValueText;

    QLabel *mLblTitle;
    QLabel *mLblValue;
    QLabel *mLblSecondaryValue;

    QColor mSuccessColor;
    QColor mWarningColor;
    QColor mAccentColor;
    QColor mDestructiveColor;
    QColor mTrackColor;
    QColor mTextColor;
    QColor mSecondaryTextColor;
};

#endif // VUMETER_TILE_H
