#include <QtTest>
#include "sidebar_section.h"

using namespace SidebarSectionState;

class TestSidebarSectionState : public QObject
{
    Q_OBJECT

    QList<Key> keys() const
    {
        return { {"monitor", "MONITOR", true}, {"manage", "MANAGE", false}, {"system", "SYSTEM", false} };
    }

private slots:
    void roundTripsById()
    {
        QHash<QString, bool> in { {"monitor", true}, {"manage", true}, {"system", false} };
        bool migrated = true;
        const QHash<QString, bool> out = fromJson(toJson(keys(), in), keys(), &migrated);
        QVERIFY(!migrated);
        QCOMPARE(out.value("manage"), true);
        QCOMPARE(out.value("system"), false);
    }

    void headerlessSectionIsNeverPersisted()
    {
        QHash<QString, bool> in { {"monitor", true} };
        QVERIFY(!toJson(keys(), in).contains("monitor"));
        QVERIFY(!fromJson(R"({"monitor":true})", keys()).contains("monitor"));
    }

    void legacyTranslatedKeysMigrate()
    {
        bool migrated = false;
        const QHash<QString, bool> out = fromJson(R"({"MANAGE":true,"SYSTEM":false})", keys(), &migrated);
        QVERIFY(migrated);
        QCOMPARE(out.value("manage"), true);
        QCOMPARE(out.value("system"), false);
    }

    void idWinsOverLegacyKey()
    {
        bool migrated = true;
        const QHash<QString, bool> out = fromJson(R"({"manage":false,"MANAGE":true})", keys(), &migrated);
        QVERIFY(!migrated);
        QCOMPARE(out.value("manage"), false);
    }

    void keysFromAnotherLanguageAreIgnored()
    {
        bool migrated = true;
        const QHash<QString, bool> out = fromJson(R"({"VERWALTEN":true})", keys(), &migrated);
        QVERIFY(!migrated);
        QVERIFY(out.isEmpty());
    }

    void regroupedSectionsInheritTheirOldGroup()
    {
        const QList<Key> regrouped = { {"clean", "CLEAN", false}, {"manage", "MANAGE", false}, {"tools", "TOOLS", false} };
        const QHash<QString, QString> inherits { {"clean", "manage"}, {"tools", "system"} };
        bool migrated = false;
        QHash<QString, bool> out = fromJson(R"({"manage":true,"system":false})", regrouped, &migrated, inherits);
        QVERIFY(migrated);
        QCOMPARE(out.value("clean"), true);
        QCOMPARE(out.value("manage"), true);
        QCOMPARE(out.value("tools"), false);

        // Once the new ids are saved, the old group no longer overrides them.
        migrated = true;
        out = fromJson(R"({"clean":false,"manage":true,"tools":true})", regrouped, &migrated, inherits);
        QVERIFY(!migrated);
        QCOMPARE(out.value("clean"), false);
    }

    void garbageJsonYieldsNoState()
    {
        QVERIFY(fromJson("not json", keys()).isEmpty());
    }
};

QTEST_MAIN(TestSidebarSectionState)
#include "test_sidebar_section_state.moc"
