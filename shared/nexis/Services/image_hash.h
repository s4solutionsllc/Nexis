#ifndef IMAGE_HASH_H
#define IMAGE_HASH_H

#include <QImage>
#include <QString>
#include <QtGlobal>

#include <optional>

// SSO-26638: in-house 64-bit difference hash (dHash) for near-duplicate
// image detection. The algorithm choice sits behind this enum so a future
// perceptual-hash algorithm (e.g. pHash) can be added without changing
// DuplicateFinderService's API — see the engineering spec's D1 for why
// dHash was picked over pHash and over a third-party library.
enum class ImageHashAlgorithm { DHash };

namespace ImageHash {

// Scales `image` to 9x8 grayscale and sets each bit to whether a pixel is
// brighter than its right-hand neighbour, producing a 64-bit fingerprint.
// Robust to resizing and re-encoding; not to rotation, cropping, or flips.
quint64 dHash(const QImage &image);

// Decodes `path` straight to the 9x8 size the hash needs, so a full-size
// bitmap of a large photo is never held in memory, and guards against
// decompression bombs via an allocation limit and a header-reported
// dimension cap. Returns nullopt for an undecodable/corrupt file or one
// that fails those guards — callers must treat that as "skip", not fatal.
std::optional<quint64> hashFile(const QString &path);

// qPopulationCount(a ^ b); range 0-64.
int hammingDistance(quint64 a, quint64 b);

} // namespace ImageHash

#endif // IMAGE_HASH_H
