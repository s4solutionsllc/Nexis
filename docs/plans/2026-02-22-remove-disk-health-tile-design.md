# Design: Remove Disk Health Tile, Enhance Disk Tile Badge

## Problem

The Dashboard has two disk-related widgets: a **Disk Tile** (usage donut) and a **Disk Health Tile** (MetricTile with progress bar + sparkline). The Health Tile's sparkline adds little value because drive health changes over weeks/months, not within a session. Meanwhile, the Disk Tile already shows per-drive health verdict badges. The Health Tile is redundant clutter.

## Decision

Remove the standalone Disk Health Tile. Enhance the existing Disk Tile health badges to include the numeric health percentage (e.g., `"Apple SSD: Good (92%)"`) so no information is lost.

## Changes

1. **`DiskTile::setDriveHealth()`** — add `healthPercent` parameter. When >= 0, append `" (N%)"` to the status label text.
2. **`dashboard_page.cpp`** — remove `mDiskHealthTile` from grid placement, drop shadows, and conditional visibility. Update `onDiskHealthUpdated()` to pass `healthPercent` to `setDriveHealth()` instead of updating the removed tile.
3. **`dashboard_page.h`** — remove `mDiskHealthTile` member.
4. **Tray alert** — remains unchanged; still fires from `onDiskHealthUpdated()`.
5. **Settings page** — the "Disk Health Alert" toggle stays (it controls tray notifications, not the tile).
6. **Theme tokens** — `@diskHealthColor` can be left in place (unused tokens are harmless, removing risks breaking if referenced elsewhere).
