#include "hero_card.h"
#include "metric_tile.h"

#include <QHBoxLayout>
#include <QFrame>

HeroCard::HeroCard(MetricTile *left, MetricTile *right, QWidget *parent)
    : QWidget(parent), mLeft(left), mRight(right)
{
    setObjectName("heroCard");
    buildLayout();
}

void HeroCard::buildLayout()
{
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    mLeft->setObjectName("heroCardLeft");
    mRight->setObjectName("heroCardRight");

    mLeft->setParent(this);
    mRight->setParent(this);

    layout->addWidget(mLeft, 1);

    auto *divider = new QFrame(this);
    divider->setObjectName("heroCardDivider");
    divider->setFrameShape(QFrame::VLine);
    divider->setFixedWidth(1);
    layout->addWidget(divider);

    layout->addWidget(mRight, 1);
}
