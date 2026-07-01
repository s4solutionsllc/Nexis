#include <QtTest>
#include "Pages/Dashboard/drive_tile_format.h"

class TestDriveTileFormat : public QObject
{
    Q_OBJECT

private slots:
    void usageText_joinsWithSlash()
    {
        QCOMPARE(DriveTileFormat::usageText("200.2 GiB", "219.0 GiB"),
                 QStringLiteral("200.2 GiB / 219.0 GiB"));
    }

    void usageText_emptyTotal_stillJoins()
    {
        QCOMPARE(DriveTileFormat::usageText("6.0 TiB", "9.0 TiB"),
                 QStringLiteral("6.0 TiB / 9.0 TiB"));
    }
};

QTEST_MAIN(TestDriveTileFormat)
#include "test_drive_tile_format.moc"
