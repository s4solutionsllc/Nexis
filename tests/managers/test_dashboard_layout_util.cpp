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

    void columnsForWidth_clampsAndDivides()
    {
        QCOMPARE(columnsForWidth(1680), 13); // floor((1680+10)/130)=13
        QCOMPARE(columnsForWidth(100), kMinCols);   // tiny window -> MIN(4)
        QCOMPARE(columnsForWidth(100000), kMaxCols); // huge -> MAX(16)
    }

    void reflow_clampsColSpanToCols()
    {
        QJsonObject t; t["id"]="disk"; t["row"]=0; t["col"]=0; t["rowSpan"]=2; t["colSpan"]=4;
        QJsonArray in; in.append(t);
        QJsonArray out = reflow(in, 3);            // only 3 columns available
        QJsonObject o = out.at(0).toObject();
        QCOMPARE(o["colSpan"].toInt(), 3);          // clamped to cols
        QCOMPARE(o["col"].toInt(), 0);
        QCOMPARE(o["row"].toInt(), 0);
    }

    void reflow_repacksWithoutOverlap()
    {
        // Two 2x2 tiles that both claim (0,0) in a 4-col grid: second must move.
        auto mk=[](const QString &id,int r,int c){ QJsonObject o; o["id"]=id; o["row"]=r; o["col"]=c; o["rowSpan"]=2; o["colSpan"]=2; return o; };
        QJsonArray in; in.append(mk("a",0,0)); in.append(mk("b",0,0));
        QJsonArray out = reflow(in, 4);
        QJsonObject a=out.at(0).toObject(), b=out.at(1).toObject();
        QCOMPARE(a["row"].toInt(),0); QCOMPARE(a["col"].toInt(),0);
        QCOMPARE(b["row"].toInt(),0); QCOMPARE(b["col"].toInt(),2); // packed to the right
    }

    void reflow_wrapsToNextRowWhenWidthExhausted()
    {
        auto mk=[](const QString &id){ QJsonObject o; o["id"]=id; o["row"]=0; o["col"]=0; o["rowSpan"]=2; o["colSpan"]=2; return o; };
        QJsonArray in; for (const char *id : {"a","b","c"}) in.append(mk(id));
        QJsonArray out = reflow(in, 4); // 4 cols -> two 2x2 per row
        QCOMPARE(out.at(0).toObject()["col"].toInt(), 0);
        QCOMPARE(out.at(1).toObject()["col"].toInt(), 2);
        QCOMPARE(out.at(2).toObject()["row"].toInt(), 2); // wrapped to next row
        QCOMPARE(out.at(2).toObject()["col"].toInt(), 0);
    }

    void isMultiInstanceType_knownTypes()
    {
        QVERIFY(isMultiInstanceType("temp"));
        QVERIFY(isMultiInstanceType("fan"));
        QVERIFY(isMultiInstanceType("disk"));
        QVERIFY(isMultiInstanceType("gpu"));
        QVERIFY(isMultiInstanceType("network"));
        QVERIFY(!isMultiInstanceType("cpu"));
        QVERIFY(!isMultiInstanceType("memory"));
        QVERIFY(!isMultiInstanceType("health"));
    }

    void typeOfUid_splitsOnHash()
    {
        QCOMPARE(typeOfUid("temp"), QString("temp"));
        QCOMPARE(typeOfUid("temp#2"), QString("temp"));
        QCOMPARE(typeOfUid("fan#10"), QString("fan"));
    }

    void makeUid_firstUnused()
    {
        QStringList used { "temp", "temp#1", "cpu" };
        QCOMPARE(makeUid(used, "fan"), QString("fan"));
        QCOMPARE(makeUid(used, "temp"), QString("temp#2"));
    }

    void usedInputsForType_filtersByType()
    {
        QJsonArray tiles;
        QJsonObject a; a["id"] = "temp"; a["input"] = "s1"; tiles.append(a);
        QJsonObject b; b["id"] = "temp"; b["input"] = "s2"; tiles.append(b);
        QJsonObject c; c["id"] = "fan";  c["input"] = "f1"; tiles.append(c);
        QStringList got = usedInputsForType(tiles, "temp");
        QCOMPARE(got.size(), 2);
        QVERIFY(got.contains("s1"));
        QVERIFY(got.contains("s2"));
    }
};

QTEST_APPLESS_MAIN(TestDashboardLayoutUtil)
#include "test_dashboard_layout_util.moc"
