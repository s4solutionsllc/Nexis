// SSO-24963: DiskMapView::higherContrastColour() — the WCAG contrast pick
// used to choose a readable label colour when a label is painted directly
// on a themed, coloured shape (tile, frame header strip, bubble, wedge)
// instead of the page background. Pure static function, so it's tested
// directly without standing up a QWidget/QApplication.

#include <QtTest>
#include <QColor>

#include "Pages/DiskMap/disk_map_view.h"

class TestDiskMapLabelContrast : public QObject
{
    Q_OBJECT
private slots:
    void picksTextColourOnDarkFill();
    void picksBackgroundColourOnLightFill();
    void isSymmetricAroundMidGrey();
};

void TestDiskMapLabelContrast::picksTextColourOnDarkFill()
{
    // Light theme: near-white text reserved for dark fills, near-black
    // background colour would be unreadable... but here the fill is dark,
    // so light text wins regardless of which theme these came from.
    const QColor lightText(240, 240, 245);
    const QColor darkBackground(20, 22, 30);
    const QColor darkFill(30, 40, 90); // deep blue tile
    QCOMPARE(DiskMapView::higherContrastColour(darkFill, lightText, darkBackground), lightText);
}

void TestDiskMapLabelContrast::picksBackgroundColourOnLightFill()
{
    // Dark theme: light text is the "normal" text colour, dark background
    // is the fallback. Against a bright fill, the dark colour reads better.
    const QColor lightText(235, 235, 240);
    const QColor darkBackground(18, 18, 22);
    const QColor brightFill(230, 225, 200); // pale yellow tile
    QCOMPARE(DiskMapView::higherContrastColour(brightFill, lightText, darkBackground), darkBackground);
}

void TestDiskMapLabelContrast::isSymmetricAroundMidGrey()
{
    const QColor black(0, 0, 0);
    const QColor white(255, 255, 255);
    QCOMPARE(DiskMapView::higherContrastColour(black, white, black), white);
    QCOMPARE(DiskMapView::higherContrastColour(white, white, black), black);
}

QTEST_APPLESS_MAIN(TestDiskMapLabelContrast)
#include "test_disk_map_label_contrast.moc"
