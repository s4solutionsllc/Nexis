#ifndef DASHBOARD_LAYOUT_UTIL_H
#define DASHBOARD_LAYOUT_UTIL_H

#include <QJsonArray>

// Pure, UI-free helpers for the dashboard bento layout. Kept separate from
// DashboardPage so the grid math and persistence migration are unit-testable
// without instantiating the full page (which builds its widget tree in init()).
namespace DashboardLayout {

// Current bento grid resolution. GH#191: doubled from the legacy 4x4 so users
// can place more, smaller tiles; default tiles span 2x2 to look unchanged.
inline constexpr int kGridRows = 8;
inline constexpr int kGridCols = 8;

// Legacy grid resolution used by persisted v1 (unversioned) layouts.
inline constexpr int kLegacyGridRows = 4;
inline constexpr int kLegacyGridCols = 4;

// Persisted layout schema version. v1 == bare JSON array in 4x4 coords (no
// envelope). v2 == {"version":2,"tiles":[...]} in 8x8 coords.
inline constexpr int kSchemaVersion = 2;

// Display tier for a tile, derived from its grid span area (rowSpan*colSpan).
// Ordered smallest..largest. Compact is the GH#191 small-tile tier.
enum DisplayTier { Compact = 0, Normal = 1, Large = 2, Hero = 3 };

// Maps a span area to a display tier. Thresholds chosen so a 2x2 default tile
// (area 4) renders as Normal (the legacy 1x1 look), 4x4 as Hero, and a 1x1 /
// 1x2 small tile as Compact.
DisplayTier tierForArea(int area);

// Returns a layout-tile array in current (8x8) coordinates. If declaredVersion
// is already current (>= kSchemaVersion) the array is returned unchanged.
// Otherwise it is treated as a legacy 4x4 layout: row/col/rowSpan/colSpan are
// scaled by kGridCols/kLegacyGridCols and clamped to the 8x8 bounds.
QJsonArray migrate(const QJsonArray &tiles, int declaredVersion);

} // namespace DashboardLayout

#endif // DASHBOARD_LAYOUT_UTIL_H
