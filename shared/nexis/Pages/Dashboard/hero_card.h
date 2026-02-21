#ifndef HERO_CARD_H
#define HERO_CARD_H

#include <QWidget>

class MetricTile;

class HeroCard : public QWidget
{
    Q_OBJECT

public:
    explicit HeroCard(MetricTile *left, MetricTile *right, QWidget *parent = nullptr);

private:
    void buildLayout();
    MetricTile *mLeft;
    MetricTile *mRight;
};

#endif // HERO_CARD_H
