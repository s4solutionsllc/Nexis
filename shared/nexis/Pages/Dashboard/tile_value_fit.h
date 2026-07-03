#ifndef TILE_VALUE_FIT_H
#define TILE_VALUE_FIT_H

#include <QFont>
#include <QString>

// GH#214: pure helper that fits a tile's centered value text to the clear
// inner width of its gauge/ring/donut. Returns the largest pixel size
// <= idealPx whose rendered advance fits maxWidth, never below minPx.
namespace TileValueFit
{
int fittedPixelSize(const QFont &base, const QString &text,
                    int maxWidth, int idealPx, int minPx = 10);
}

#endif // TILE_VALUE_FIT_H
