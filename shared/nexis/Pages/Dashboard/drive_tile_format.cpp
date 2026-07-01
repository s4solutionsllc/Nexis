#include "drive_tile_format.h"

namespace DriveTileFormat {

QString usageText(const QString &used, const QString &total)
{
    return QStringLiteral("%1 / %2").arg(used, total);
}

} // namespace DriveTileFormat
