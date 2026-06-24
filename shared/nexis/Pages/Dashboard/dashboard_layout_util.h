#ifndef DASHBOARD_LAYOUT_UTIL_H
#define DASHBOARD_LAYOUT_UTIL_H

#include <QJsonArray>

// Pure, UI-free helpers for the dashboard bento layout. Kept separate from
// DashboardPage so the grid math and persistence migration are unit-testable
// without instantiating the full page (which builds its widget tree in init()).
namespace DashboardLayout {

// Fixed bento cell geometry (GH#191 revised model — fixed cells, responsive
// columns). A 1x1 cell is kCellW x kCellH with kGap between cells.
inline constexpr int kCellW = 120;
inline constexpr int kCellH = 98;
inline constexpr int kGap = 10;

// Responsive column clamp.
inline constexpr int kMinCols = 4;
inline constexpr int kMaxCols = 16;

// Legacy (v1, 4x4) layouts scale by this factor so an old 1x1 becomes a 2x2.
inline constexpr int kLegacyScale = 2;

inline constexpr int kSchemaVersion = 2;

// Display tier for a tile, derived from its grid span area (rowSpan*colSpan).
// Ordered smallest..largest. Compact is the GH#191 small-tile tier.
enum DisplayTier { Compact = 0, Normal = 1, Large = 2, Hero = 3 };

// Maps a span area to a display tier. Thresholds chosen so a 2x2 default tile
// (area 4) renders as Normal (the legacy 1x1 look), 4x4 as Hero, and a 1x1 /
// 1x2 small tile as Compact.
DisplayTier tierForArea(int area);

// Responsive visible column count for a given panel (viewport) width.
int columnsForWidth(int panelWidth);

// Repacks tiles row-major into the first free region of a `cols`-wide grid:
// colSpan is clamped to <= cols, rowSpan >= 1, and row/col are rewritten so no
// two tiles overlap and tiles fill from the top-left. Input order is preserved
// as packing priority. Rows are unbounded.
QJsonArray reflow(const QJsonArray &tiles, int cols);

// Returns a layout-tile array in current schema coordinates. If declaredVersion
// is already current (>= kSchemaVersion) the array is returned unchanged.
// Otherwise it is treated as a legacy v1 layout: row/col/rowSpan/colSpan are
// scaled by kLegacyScale and clamped to current bounds.
QJsonArray migrate(const QJsonArray &tiles, int declaredVersion);

} // namespace DashboardLayout

#endif // DASHBOARD_LAYOUT_UTIL_H
