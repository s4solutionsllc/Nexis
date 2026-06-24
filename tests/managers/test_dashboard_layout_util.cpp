#include <QtTest>
#include <QJsonArray>
#include <QJsonObject>
#include "Pages/Dashboard/dashboard_layout_util.h"

using namespace DashboardLayout;

class TestDashboardLayoutUtil : public QObject
{
    Q_OBJECT

private slots:
    void tierForArea_boundaries()
    {
        QCOMPARE(tierForArea(1), Compact);
        QCOMPARE(tierForArea(3), Compact);
        QCOMPARE(tierForArea(4), Normal);
        QCOMPARE(tierForArea(7), Normal);
        QCOMPARE(tierForArea(8), Large);
        QCOMPARE(tierForArea(15), Large);
        QCOMPARE(tierForArea(16), Hero);
        QCOMPARE(tierForArea(64), Hero);
    }

    void migrate_v1_scalesCoordsByTwo()
    {
        QJsonObject t;
        t["id"] = "cpu";
        t["row"] = 1; t["col"] = 2; t["rowSpan"] = 1; t["colSpan"] = 1;
        QJsonArray in; in.append(t);

        QJsonArray out = migrate(in, 1);
        QCOMPARE(out.size(), 1);
        QJsonObject o = out.at(0).toObject();
        QCOMPARE(o["row"].toInt(), 2);
        QCOMPARE(o["col"].toInt(), 4);
        QCOMPARE(o["rowSpan"].toInt(), 2);
        QCOMPARE(o["colSpan"].toInt(), 2);
        QCOMPARE(o["id"].toString(), QString("cpu"));
    }

    void migrate_v1_clampsToBounds()
    {
        QJsonObject t;
        t["id"] = "disk";
        t["row"] = 3; t["col"] = 3; t["rowSpan"] = 1; t["colSpan"] = 1;
        QJsonArray in; in.append(t);

        QJsonArray out = migrate(in, 1);
        QJsonObject o = out.at(0).toObject();
        // 3*2 = 6, span 2 -> ends at row 8 == kGridRows, still in bounds.
        QCOMPARE(o["row"].toInt(), 6);
        QCOMPARE(o["col"].toInt(), 6);
        QCOMPARE(o["rowSpan"].toInt(), 2);
        QCOMPARE(o["colSpan"].toInt(), 2);
    }

    void migrate_v2_unchanged()
    {
        QJsonObject t;
        t["id"] = "fan"; t["input"] = "k10temp/fan1";
        t["row"] = 5; t["col"] = 1; t["rowSpan"] = 1; t["colSpan"] = 1;
        QJsonArray in; in.append(t);

        QJsonArray out = migrate(in, 2);
        QJsonObject o = out.at(0).toObject();
        QCOMPARE(o["row"].toInt(), 5);
        QCOMPARE(o["col"].toInt(), 1);
        QCOMPARE(o["input"].toString(), QString("k10temp/fan1"));
    }
};

QTEST_APPLESS_MAIN(TestDashboardLayoutUtil)
#include "test_dashboard_layout_util.moc"
