#ifndef HEALTH_SCORE_TILE_H
#define HEALTH_SCORE_TILE_H

#include "metric_tile_base.h"
#include "health_score_calculator.h"

#include <QLabel>
#include <QPushButton>

class HealthScoreTile : public MetricTileBase
{
    Q_OBJECT

public:
    explicit HealthScoreTile(const QString &colorToken, QWidget *parent = nullptr);

    void setValue(int percent, const QString &valueText) override;
    void addDataPoint(double value) override;
    void setSubtitle(const QString &text) override;
    void setTrendDirection(TrendDirection dir) override;
    void setSecondaryValue(const QString &text) override;
    void setDisplayMode(DisplayMode mode) override;
    void setQuickAction(const QString &text, std::function<void()> callback) override;
    void refreshThemeColors() override;

    HealthScoreCalculator *calculator() { return &mCalculator; }
    void recalculate();

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void buildLayout();
    void paintBreakdownBars(QPainter &painter);

    HealthScoreCalculator mCalculator;

    QLabel      *mLblTitle;
    QLabel      *mLblScore;
    QLabel      *mLblScoreLabel;
    QPushButton *mBtnAction = nullptr;

    int mCurrentScore;
};

#endif // HEALTH_SCORE_TILE_H
