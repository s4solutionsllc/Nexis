#ifndef TRAY_MENU_MODEL_H
#define TRAY_MENU_MODEL_H

#include <QList>
#include <QString>
#include <QSystemTrayIcon>
#include "sidebar_section.h"

// One block of the tray menu: either a flat run of items (headerless, e.g.
// MONITOR) or a submenu with a title (e.g. MANAGE -> "Manage").
struct TrayMenuGroup {
    QString name;
    bool headerless = false;
    QList<QPushButton*> items;
};

// SSO-23896: derives the tray menu's grouping from the same section model
// that drives the sidebar, so the two surfaces cannot drift. Hidden buttons
// (platform-unavailable pages) are excluded and a group left with no visible
// members is dropped entirely rather than rendered as a dead submenu.
QList<TrayMenuGroup> buildTrayMenuGroups(const QList<SidebarSection> &sections);

// Renders a sidebar section name (e.g. "MANAGE") as a tray submenu title
// ("Manage") — the sidebar uses all-caps section headers, but a native OS
// menu submenu reads as a normal title-case row.
QString trayMenuGroupTitle(const QString &sectionName);

// SSO-26762: pure per-platform answer for whether a tray-icon activation
// reason should raise the main window. macOS never does — Qt reports every
// status-item click as Trigger even when a context menu is shown and Qt is
// already popping it up, so raising there would surface the window
// alongside the menu; "Open Nexis" in the menu is the only way in. Linux
// keeps the pre-existing split: left-click/double-click opens, right-click/
// middle-click shows the menu only.
bool trayActivationOpensWindow(QSystemTrayIcon::ActivationReason reason, bool isMacPlatform);

#endif // TRAY_MENU_MODEL_H
