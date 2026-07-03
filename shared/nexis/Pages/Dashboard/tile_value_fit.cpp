#include "tile_value_fit.h"

#include <QFontMetrics>

namespace TileValueFit
{

int fittedPixelSize(const QFont &base, const QString &text,
                    int maxWidth, int idealPx, int minPx)
{
    if (idealPx < minPx)
        idealPx = minPx;
    if (text.isEmpty() || maxWidth <= 0)
        return idealPx;

    QFont font(base);
    font.setPixelSize(idealPx);
    int advance = QFontMetrics(font).horizontalAdvance(text);
    if (advance <= maxWidth)
        return idealPx;

    // Proportional first guess, then step down: advance is not perfectly
    // linear in pixel size (hinting), so verify and correct.
    int px = qMax(minPx, idealPx * maxWidth / advance);
    font.setPixelSize(px);
    while (px > minPx && QFontMetrics(font).horizontalAdvance(text) > maxWidth) {
        --px;
        font.setPixelSize(px);
    }
    return px;
}

}
