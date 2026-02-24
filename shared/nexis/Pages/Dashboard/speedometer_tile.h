#ifndef SPEEDOMETER_TILE_H
#define SPEEDOMETER_TILE_H

#include "metric_tile_base.h"

#include <QLabel>
#include <QColor>
#include <QPushButton>

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
    QToolButton *gearButton() override;
    void setGearVisible(bool visible) override;
    void refreshThemeColors() override;

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void buildLayout();
    void updateGearIcon();
    void updateTrend();
    QColor gradientColorAt(double fraction) const;

    int mPercent;
    QString mValueText;

    QColor mMetricColor;
    QColor mCardBgColor;
    QColor mTextColor;
    QColor mSecondaryTextColor;
    QColor mGreenColor;
    QColor mYellowColor;
    QColor mOrangeColor;
    QColor mRedColor;

    QLabel *mLblTitle;
    QLabel *mLblValue;
    QLabel *mLblSecondaryValue;
    QLabel *mLblSubtitle;
    QLabel *mLblTrend;
    QPushButton *mBtnAction;
    QToolButton *mGearButton;

    TrendDirection mCurrentTrend;
};

#endif // SPEEDOMETER_TILE_H
