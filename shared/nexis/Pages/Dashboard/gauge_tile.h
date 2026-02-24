#ifndef GAUGE_TILE_H
#define GAUGE_TILE_H

#include "metric_tile_base.h"

#include <QLabel>
#include <QColor>
#include <QPushButton>

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

    QColor mArcColor;
    QColor mArcEndColor;
    QColor mTrackColor;
    QColor mTextColor;
    QColor mSecondaryTextColor;

    int mPercent;
    QString mValueText;
    QString mSecondaryText;

    QLabel *mLblTitle;
    QLabel *mLblSubtitle;
    QLabel *mLblTrend;
    QPushButton *mBtnAction;
    QToolButton *mGearButton;

    TrendDirection mCurrentTrend;
};

#endif // GAUGE_TILE_H
