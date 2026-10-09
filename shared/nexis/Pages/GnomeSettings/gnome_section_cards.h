#ifndef GNOME_SECTION_CARDS_H
#define GNOME_SECTION_CARDS_H

#include <QList>
#include <QString>

class QLayout;
class QWidget;

// DS §2/§3 section-card recipe shared by the GNOME Settings tabs: a compact
// accent-bar header row above the group's content, elevated card chrome on
// the group itself, and one drop shadow per card.
namespace GnomeSectionCards {

void buildHeader(QWidget *headerContainer, const QString &title);
void applyCardChrome(QWidget *card, QLayout *content);
// Re-apply on theme change: the shadow color resolves from @shadowColor at
// call time.
void applyShadows(const QList<QWidget *> &cards);

}

#endif // GNOME_SECTION_CARDS_H
