#include "gnome_section_cards.h"
#include "utilities.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>

void GnomeSectionCards::buildHeader(QWidget *headerContainer, const QString &title)
{
    headerContainer->setObjectName("sectionHeaderRow");

    QHBoxLayout *row = new QHBoxLayout(headerContainer);
    row->setContentsMargins(14, 12, 14, 8);
    row->setSpacing(8);

    QFrame *accentBar = new QFrame(headerContainer);
    accentBar->setObjectName("sectionHeaderAccent");
    accentBar->setProperty("compact", true);
    accentBar->setProperty("accentToken", "accent");
    accentBar->setFrameShape(QFrame::NoFrame);
    accentBar->setFixedWidth(3);
    accentBar->setMinimumHeight(18);
    accentBar->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    row->addWidget(accentBar);

    QLabel *lblTitle = new QLabel(title, headerContainer);
    lblTitle->setObjectName("sectionHeaderTitle");
    row->addWidget(lblTitle);
    row->addStretch();
}

void GnomeSectionCards::applyCardChrome(QWidget *card, QLayout *content)
{
    card->setAttribute(Qt::WA_StyledBackground, true);
    card->setProperty("cardRole", "elevated");
    // The header row above supplies its own top padding.
    content->setContentsMargins(14, 0, 14, 14);
}

void GnomeSectionCards::applyShadows(const QList<QWidget *> &cards)
{
    for (QWidget *card : cards)
        Utilities::addDropShadow(card, 90, 26);
}
