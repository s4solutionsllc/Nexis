# Uninstaller (sidebar label "Applications") — prototype rationale

**Status:** Approved by maintainer (2026-07-15).

**Before:** current-state/macos/dark/uninstaller.png
**After:** renders/uninstaller_{dark,light}.png

**Note on labelling:** the file is `uninstaller.html` but the sidebar entry for this page reads **"Applications"** on macOS (per CAPTURE_NOTES.md coverage row 7); the prototype highlights that "Applications" item, not an "Uninstaller" label.

**Changes:**
1. Wrapped the application list in one elevated container card (DS §2 recipe) — container-level shadow, flat rows inside. In the before the list sat as a flat panel barely distinct from the page. Source: DS §2 (`dashboard_tile_wrapper.cpp:107-130`) + DS §7 (shadow on the container, never per row). Evidence: renders/uninstaller_dark.png.
2. Three-layer surface hierarchy (`@pageContent` / `@cardBgElevated` container / `@cardBg` rows). Source: DS §1 (`values.ini:2,29,30`).
3. Toolbar chrome rebuilt on header-anatomy spacing (this page leads with tabs, no title): the "Applications (49)" / "Orphan Packages (0)" segmented tabs and the Search field are kept and aligned; active tab uses `@accentColor`. Source: DS §3 (`metric_tile_base.cpp:255-307`); DS §4 (`style.qss:772-815`).
4. The "Application" column header is frozen (`@cardBgElevated`, `@borderColor` bottom rule) and the "▸ Applications (49)" group row is a flat `@cardBg`-eligible row inside the elevated card. Source: DS §7 (`style.qss:328-350`); [NNG-TABLES].

**Explicitly unchanged:** sidebar; the two tabs and their counts; the collapsed "Applications (49)" tree group (kept collapsed, as captured — no child rows invented); the Search field; the centered "Uninstall Selected" primary action at the bottom.
