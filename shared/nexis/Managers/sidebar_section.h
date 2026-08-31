#ifndef SIDEBAR_SECTION_H
#define SIDEBAR_SECTION_H

#include <QString>
#include <QList>

class QPushButton;
class QWidget;
class QVBoxLayout;

// One collapsible group in the sidebar nav (e.g. "MANAGE"). MONITOR is the
// only headerless section — always expanded, no toggle. Also the single
// source of truth for tray menu grouping (SSO-23896): the tray derives its
// structure from this, so adding a sidebar page never needs a tray-side edit.
struct SidebarSection {
    QString name;
    QPushButton *header = nullptr;
    QWidget *container = nullptr;
    QVBoxLayout *containerLayout = nullptr;
    QList<QPushButton*> buttons;
    bool collapsed = false;
    bool headerless = false;
};

#endif // SIDEBAR_SECTION_H
