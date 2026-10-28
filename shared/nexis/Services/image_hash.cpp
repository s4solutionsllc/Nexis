#include "image_hash.h"

#include <QImageReader>

namespace {

constexpr int kHashWidth = 9;
constexpr int kHashHeight = 8;
// SSO-26638 security note: stricter than Qt's 256 MB default, and the
// files hashed here are untrusted (Downloads, mounted media, browser
// caches) — see the spec's threat model.
constexpr int kAllocationLimitMB = 128;
constexpr int kMaxDimensionPx = 20000;

} // namespace

quint64 ImageHash::dHash(const QImage &image)
{
    const QImage scaled = image.convertToFormat(QImage::Format_Grayscale8)
                               .scaled(kHashWidth, kHashHeight,
                                       Qt::IgnoreAspectRatio,
                                       Qt::SmoothTransformation);

    quint64 hash = 0;
    int bit = 0;
    for (int y = 0; y < kHashHeight; ++y) {
        const uchar *row = scaled.constScanLine(y);
        for (int x = 0; x < kHashWidth - 1; ++x) {
            if (row[x] > row[x + 1])
                hash |= (quint64(1) << bit);
            ++bit;
        }
    }
    return hash;
}

std::optional<quint64> ImageHash::hashFile(const QString &path)
{
    QImageReader reader(path);
    reader.setAutoTransform(true);
    reader.setAllocationLimit(kAllocationLimitMB);

    const QSize headerSize = reader.size();
    if (headerSize.isValid() &&
        (headerSize.width() > kMaxDimensionPx || headerSize.height() > kMaxDimensionPx))
        return std::nullopt;

    // Decode directly to the hash's target size: the full-resolution
    // bitmap of a large photo is never materialized.
    reader.setScaledSize(QSize(kHashWidth, kHashHeight));
    const QImage image = reader.read();
    if (image.isNull())
        return std::nullopt;

    return dHash(image);
}

int ImageHash::hammingDistance(quint64 a, quint64 b)
{
    return qPopulationCount(a ^ b);
}
