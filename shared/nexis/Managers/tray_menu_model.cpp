#include "tray_menu_model.h"

#include <QPushButton>
#include <QStringList>

QList<TrayMenuGroup> buildTrayMenuGroups(const QList<SidebarSection> &sections)
{
    QList<TrayMenuGroup> groups;

    for (const SidebarSection &section : sections) {
        TrayMenuGroup group;
        group.name = section.name;
        group.headerless = section.headerless;

        for (QPushButton *button : section.buttons) {
            if (button && !button->isHidden())
                group.items.append(button);
        }

        if (group.items.isEmpty())
            continue;

        groups.append(group);
    }

    return groups;
}

QString trayMenuGroupTitle(const QString &sectionName)
{
    QStringList words = sectionName.toLower().split(QLatin1Char(' '), Qt::SkipEmptyParts);
    for (QString &word : words) {
        word.replace(0, 1, word.left(1).toUpper());
    }
    return words.join(QLatin1Char(' '));
}
