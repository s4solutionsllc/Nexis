#include <QtTest>
#include <QFontMetrics>
#include "Pages/Dashboard/tile_value_fit.h"

class TestTileValueFit : public QObject
{
    Q_OBJECT

private slots:
    void shortTextKeepsIdealSize()
    {
        QFont f;
        f.setBold(true);
        QCOMPARE(TileValueFit::fittedPixelSize(f, "0%", 140, 34), 34);
    }

    void wideTextShrinksToFit()
    {
        QFont f;
        f.setBold(true);
        const QString text = QStringLiteral("3846 RPM");
        const int maxWidth = 140;
        int px = TileValueFit::fittedPixelSize(f, text, maxWidth, 34);

        QVERIFY(px < 34);
        QVERIFY(px >= 10);

        QFont fitted(f);
        fitted.setPixelSize(px);
        QVERIFY(QFontMetrics(fitted).horizontalAdvance(text) <= maxWidth);
    }

    void neverShrinksBelowMinimum()
    {
        QFont f;
        f.setBold(true);
        QCOMPARE(TileValueFit::fittedPixelSize(f, "3846 RPM", 5, 34), 10);
    }

    void emptyTextReturnsIdeal()
    {
        QFont f;
        QCOMPARE(TileValueFit::fittedPixelSize(f, QString(), 140, 28), 28);
    }

    void nonPositiveWidthReturnsIdeal()
    {
        QFont f;
        QCOMPARE(TileValueFit::fittedPixelSize(f, "870 RPM", 0, 28), 28);
        QCOMPARE(TileValueFit::fittedPixelSize(f, "870 RPM", -12, 28), 28);
    }

    void idealBelowMinimumClampsUp()
    {
        QFont f;
        QCOMPARE(TileValueFit::fittedPixelSize(f, "0%", 140, 8, 10), 10);
    }
};

QTEST_MAIN(TestTileValueFit)
#include "test_tile_value_fit.moc"
