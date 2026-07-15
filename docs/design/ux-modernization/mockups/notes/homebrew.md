# Homebrew — prototype rationale

**Status:** Approved by maintainer (2026-07-15), including the flagged maintainer-judgment item(s).

**Before:** current-state/macos/dark/homebrew.png
**After:** renders/homebrew_{dark,light}.png

**Changes:**
1. Wrapped the "Available Updates" table and the "Homebrew Packages" tree each in one DS §2 elevated container card (fill `@cardBgElevated`, 1px `@borderColor`, radius 12, single container-level shadow); update rows and the collapsible tree-group rows are flat inside their container. Source: DS §2 (`dashboard_tile_wrapper.cpp:107-130`) + DS §7 (shadow on the container, never per row). Evidence: renders/homebrew_dark.png.
2. Three-layer surface hierarchy — page `@pageContent`, containers `@cardBgElevated`, flat rows `@cardBg`. In the before the tables sat as flat bands barely distinct from the page. Source: DS §1 (`values.ini:2,29,30`).
3. Rebuilt the top of the page on header anatomy: a 3px accent bar + "Available Updates (3)" title + muted "Outdated Homebrew packages" source line, keeping the captured "Check Now" action; the second section gets a matching accent-bar "Homebrew Packages (171)" header with the captured "Search packages" field. Source: DS §3 (`metric_tile_base.cpp:255-307`); DS §4 (`style.qss:772-815`).
4. Frozen sticky headers + right-aligned tabular Version column for the updates table [NNG-TABLES]; the two package groups ("Homebrew Cask (6)", "Homebrew Formula (165)") keep their collapsed tree state with the disclosure caret, as captured. Source: DS §7 (`style.qss:328-350`); [NNG-TABLES] (sources.md).

**Font note:** the update-row and tree cells (source/package/version) use the fixed monospace stack `"JetBrains Mono", ui-monospace, monospace` (`@monoFontFamily` QSS string, not a `values.ini` token). Source: `app_manager.cpp:208-209`.

**Explicitly unchanged:** sidebar contents/order (Homebrew highlighted with badge 3 — the correct page highlight, not the stale "Dashboard" one, per CAPTURE_NOTES.md gotcha #5); the "Available Updates (3)" table columns (Source / Package / Version) and its three brew rows (dav1d 1.5.4, ruby 4.0.6, blender 5.2.0); the "Check Now" action; the "Homebrew Packages (171)" tree with its two collapsed groups and counts; the "Search packages" field; the bottom Install / Uninstall controls; no invented rows or expanded children.
