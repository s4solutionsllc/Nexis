// SSO-26638: ImageHash::dHash/hashFile — the 64-bit difference-hash
// primitive behind Duplicate Finder's similar-image mode. Fixtures are
// generated programmatically into a QTemporaryDir (a gradient drawn with
// QPainter), never committed as binary files.

#include <QtTest>
#include <QColor>
#include <QFile>
#include <QImage>
#include <QLinearGradient>
#include <QPainter>
#include <QTemporaryDir>

#include "Services/image_hash.h"

namespace {

// A smooth diagonal gradient — enough low-frequency structure for dHash's
// neighbour-pixel comparison to be meaningful, and it survives a 50%
// resize or JPEG re-encode without flipping many of the 64 bits.
QImage makeGradientImage(int size = 256)
{
    QImage image(size, size, QImage::Format_RGB32);
    QPainter painter(&image);
    QLinearGradient gradient(0, 0, size, size);
    gradient.setColorAt(0.0, QColor(10, 10, 10));
    gradient.setColorAt(1.0, QColor(245, 245, 245));
    painter.fillRect(image.rect(), gradient);
    painter.end();
    return image;
}

// A visually distinct image: a checkerboard has sharp, high-frequency
// edges in both axes, the opposite of the smooth gradient above. A cell
// size of size/8 keeps big, cleanly-alternating blocks under dHash's 9x8
// downsample, instead of averaging away into near-uniform grey.
QImage makeCheckerboardImage(int size = 256, int cell = 32)
{
    QImage image(size, size, QImage::Format_RGB32);
    QPainter painter(&image);
    painter.fillRect(image.rect(), Qt::white);
    painter.setBrush(Qt::black);
    painter.setPen(Qt::NoPen);
    for (int y = 0; y < size; y += cell) {
        for (int x = 0; x < size; x += cell) {
            if (((x / cell) + (y / cell)) % 2 == 0)
                painter.drawRect(x, y, cell, cell);
        }
    }
    painter.end();
    return image;
}

} // namespace

class TestImageHash : public QObject
{
    Q_OBJECT

private slots:
    void dHash_sameImage_isIdentical();
    void dHash_pngToJpegReencode_isStable();
    void dHash_halfSizeResize_withinSmallDistance();
    void dHash_visuallyDifferentImage_exceedsFifteen();
    void hashFile_corruptFile_returnsNullopt();
    void hashFile_missingFile_returnsNullopt();
    void hammingDistance_isSymmetricAndZeroForEqualHashes();
};

void TestImageHash::dHash_sameImage_isIdentical()
{
    const QImage image = makeGradientImage();
    QCOMPARE(ImageHash::dHash(image), ImageHash::dHash(image));
}

void TestImageHash::dHash_pngToJpegReencode_isStable()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    const QImage image = makeGradientImage();
    const QString pngPath = tmp.path() + "/original.png";
    const QString jpgPath = tmp.path() + "/reencoded.jpg";
    QVERIFY(image.save(pngPath, "PNG"));
    QVERIFY(image.save(jpgPath, "JPG", 85));

    const auto pngHash = ImageHash::hashFile(pngPath);
    const auto jpgHash = ImageHash::hashFile(jpgPath);
    QVERIFY(pngHash.has_value());
    QVERIFY(jpgHash.has_value());

    QVERIFY2(ImageHash::hammingDistance(*pngHash, *jpgHash) <= 4,
             "dHash must stay stable across a PNG -> JPEG(q=85) re-encode");
}

void TestImageHash::dHash_halfSizeResize_withinSmallDistance()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    const QImage image = makeGradientImage(256);
    const QImage resized = image.scaled(128, 128, Qt::IgnoreAspectRatio,
                                         Qt::SmoothTransformation);

    const QString fullPath = tmp.path() + "/full.png";
    const QString halfPath = tmp.path() + "/half.png";
    QVERIFY(image.save(fullPath, "PNG"));
    QVERIFY(resized.save(halfPath, "PNG"));

    const auto fullHash = ImageHash::hashFile(fullPath);
    const auto halfHash = ImageHash::hashFile(halfPath);
    QVERIFY(fullHash.has_value());
    QVERIFY(halfHash.has_value());

    QVERIFY2(ImageHash::hammingDistance(*fullHash, *halfHash) <= 5,
             "a 50% resize of the same image must land within distance <= 5");
}

void TestImageHash::dHash_visuallyDifferentImage_exceedsFifteen()
{
    const quint64 gradientHash = ImageHash::dHash(makeGradientImage());
    const quint64 checkerHash = ImageHash::dHash(makeCheckerboardImage());

    QVERIFY2(ImageHash::hammingDistance(gradientHash, checkerHash) > 15,
             "a visually different image must exceed distance 15");
}

void TestImageHash::hashFile_corruptFile_returnsNullopt()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    const QString corruptPath = tmp.path() + "/corrupt.jpg";
    QFile f(corruptPath);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("this is not a valid jpeg file, just garbage bytes");
    f.close();

    QVERIFY2(!ImageHash::hashFile(corruptPath).has_value(),
             "a corrupt file must be skipped, not throw or crash");
}

void TestImageHash::hashFile_missingFile_returnsNullopt()
{
    QVERIFY(!ImageHash::hashFile("/no/such/path/does-not-exist.png").has_value());
}

void TestImageHash::hammingDistance_isSymmetricAndZeroForEqualHashes()
{
    const quint64 a = 0x0F0F0F0F0F0F0F0FULL;
    const quint64 b = 0x00FF00FF00FF00FFULL;

    QCOMPARE(ImageHash::hammingDistance(a, a), 0);
    QCOMPARE(ImageHash::hammingDistance(a, b), ImageHash::hammingDistance(b, a));
}

QTEST_MAIN(TestImageHash)
#include "test_image_hash.moc"
