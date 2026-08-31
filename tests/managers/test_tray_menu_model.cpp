#include <QtTest>
#include <QPushButton>
#include <QWidget>
#include "Managers/tray_menu_model.h"

// SSO-23896: buildTrayMenuGroups() is the sole place that turns the sidebar's
// section model into the tray's grouping, so these pin the contract the tray
// rewrite depends on: order, membership, hidden-button filtering, and
// dropping empty groups.
class TestTrayMenuModel : public QObject
{
    Q_OBJECT

private slots:
    void groupOrder_matchesSectionOrder()
    {
        // Parented (non-top-level) like real sidebar buttons, so isHidden()
        // defaults to false without an explicit show() — a bare top-level
        // QPushButton defaults isHidden()==true until shown, which doesn't
        // match how sidebar buttons live inside a shown parent hierarchy.
        QWidget host;
        QPushButton dash(&host), hw(&host);
        SidebarSection monitor;
        monitor.name = "MONITOR";
        monitor.headerless = true;
        monitor.buttons = {&dash, &hw};

        QPushButton cleaner(&host), disk(&host);
        SidebarSection manage;
        manage.name = "MANAGE";
        manage.buttons = {&cleaner, &disk};

        const QList<TrayMenuGroup> groups = buildTrayMenuGroups({monitor, manage});

        QCOMPARE(groups.size(), 2);
        QCOMPARE(groups[0].name, QStringLiteral("MONITOR"));
        QCOMPARE(groups[1].name, QStringLiteral("MANAGE"));
    }

    void groupMembership_carriesSectionButtons()
    {
        QWidget host;
        QPushButton cleaner(&host), disk(&host), search(&host);
        SidebarSection manage;
        manage.name = "MANAGE";
        manage.buttons = {&cleaner, &disk, &search};

        const QList<TrayMenuGroup> groups = buildTrayMenuGroups({manage});

        QCOMPARE(groups.size(), 1);
        QCOMPARE(groups[0].items, (QList<QPushButton*>{&cleaner, &disk, &search}));
        QVERIFY(!groups[0].headerless);
    }

    void hiddenButtons_areExcluded()
    {
        QWidget host;
        QPushButton docker(&host), helpers(&host), systemLogs(&host);
        docker.hide(); // e.g. ToolManager::checkDocker() was false

        SidebarSection system;
        system.name = "SYSTEM";
        system.buttons = {&docker, &helpers, &systemLogs};

        const QList<TrayMenuGroup> groups = buildTrayMenuGroups({system});

        QCOMPARE(groups.size(), 1);
        QCOMPARE(groups[0].items, (QList<QPushButton*>{&helpers, &systemLogs}));
    }

    void emptyGroup_isDropped()
    {
        QWidget host;
        QPushButton onlyMember(&host);
        onlyMember.hide();

        SidebarSection allHidden;
        allHidden.name = "SYSTEM";
        allHidden.buttons = {&onlyMember};

        QPushButton visible(&host);
        SidebarSection manage;
        manage.name = "MANAGE";
        manage.buttons = {&visible};

        const QList<TrayMenuGroup> groups = buildTrayMenuGroups({allHidden, manage});

        QCOMPARE(groups.size(), 1);
        QCOMPARE(groups[0].name, QStringLiteral("MANAGE"));
    }

    void groupTitle_isTitleCased()
    {
        QCOMPARE(trayMenuGroupTitle(QStringLiteral("MANAGE")), QStringLiteral("Manage"));
        QCOMPARE(trayMenuGroupTitle(QStringLiteral("SYSTEM")), QStringLiteral("System"));
    }
};

QTEST_MAIN(TestTrayMenuModel)
#include "test_tray_menu_model.moc"
