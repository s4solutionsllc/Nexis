#ifndef DRIVE_TILE_FORMAT_H
#define DRIVE_TILE_FORMAT_H

#include <QString>

// Pure formatting helpers for Drive tile chrome, kept GUI-free so they can be
// unit-tested without pulling in nexis-gui (mirrors dashboard_layout_util).
namespace DriveTileFormat {

// "used / total"
QString usageText(const QString &used, const QString &total);

} // namespace DriveTileFormat

#endif // DRIVE_TILE_FORMAT_H
