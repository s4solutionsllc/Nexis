# Disk Tools — prototype rationale

**Before:** current-state/macos/dark/disk_tools.png
**After:** renders/disk_tools_{dark,light}.png

**Changes:**
1. Wrapped the scan-roots list and the results table each in one DS §2 elevated container card (fill `@cardBgElevated`, 1px `@borderColor`, radius 12, single container-level shadow) — the results table's scan-root rows and result rows are flat inside their container. Source: DS §2 (`dashboard_tile_wrapper.cpp:107-130`) + DS §7 (shadow on the container, never per row). Evidence: renders/disk_tools_dark.png.
2. Three-layer surface hierarchy — page `@pageContent`, containers `@cardBgElevated`, flat path/result rows `@cardBg`. In the before, the panels and page were near-identical tones. Source: DS §1 (`values.ini:2,29,30`).
3. Header chrome: this page leads with the captured "Large & Old Files" / "Duplicate Finder" tabs (no title label), kept and aligned on DS §3 header spacing; the active tab uses `@accentColor`. Source: DS §3 (`metric_tile_base.cpp:255-307`); DS §4 (`style.qss:772-815`).
4. Frozen sticky header + right-aligned tabular Size column for the results table [NNG-TABLES]; the pre-scan empty table is upgraded to a full empty state — disk icon + explanation + a "Scan" next-action affordance — rather than a bare header over blank space. Source: DS §7 (`style.qss:328-350`); DS §5 empty-state rule + [NNG-EMPTY] (sources.md).
5. Bottom action bar rebuilt on DS §3 spacing: the "No files selected" selection readout + "Move to Trash", control set unchanged. Source: DS §3 (`metric_tile_base.cpp:255-307`).

**Font note:** the scan-root path rows (`/Users/luke`, …) use the fixed monospace stack `"JetBrains Mono", ui-monospace, monospace` (`@monoFontFamily` QSS string, not a `values.ini` token). Source: `app_manager.cpp:208-209`.

**Explicitly unchanged:** sidebar contents/order (Disk Tools highlighted — the correct page highlight, not the stale "Dashboard" one, per CAPTURE_NOTES.md gotcha #5); the two mode tabs; the scan-roots list and its Add… / Remove buttons; the criteria controls (Size ≥ stepper + unit select, Not-accessed ≥ stepper + unit select, Match select) and their values (100 MB / 180 days / Either); the Scan button; the five result columns (Name / Path / Size / Last Accessed / Last Modified) and order; the "Move to Trash" action; no invented result rows.
