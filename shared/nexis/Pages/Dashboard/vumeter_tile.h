#ifndef VUMETER_TILE_H
#define VUMETER_TILE_H

#include "metric_tile_base.h"

#include <QLabel>
#include <QColor>
#include <QPushButton>

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

    int segmentCount() const;
    int barWidth() const;
    int scaleFontSize() const;

    QColor segmentColor(int segmentIndex, int totalSegments) const;

    int mPercent;
    QString mValueText;

    QLabel *mLblTitle;
    QLabel *mLblValue;
    QLabel *mLblSecondaryValue;
    QLabel *mLblSubtitle;
    QLabel *mLblTrend;
    QPushButton *mBtnAction;
    QToolButton *mGearButton;

    QColor mSuccessColor;
    QColor mWarningColor;
    QColor mAccentColor;
    QColor mDestructiveColor;
    QColor mTrackColor;
    QColor mTextColor;
    QColor mSecondaryTextColor;

    TrendDirection mCurrentTrend;
};

#endif // VUMETER_TILE_H
