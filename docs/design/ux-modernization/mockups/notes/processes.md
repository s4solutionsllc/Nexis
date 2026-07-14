# Processes — prototype rationale

**Before:** current-state/macos/dark/processes.png
**After:** renders/processes_{dark,light}.png

**Changes:**
1. Wrapped the bare edge-to-edge process table in one elevated container card (fill `@cardBgElevated`, 1px `@borderColor`, radius 12, single drop shadow alpha 90 / blur 26 / offset 0,2, 8px outer margin). The shadow lives on the container, never on rows. Source: DESIGN_SYSTEM.md DS §2 (`dashboard_tile_wrapper.cpp:107-130`) + DS §7 (container-level shadow). Evidence: renders/processes_dark.png.
2. Established the three-layer surface hierarchy so the table reads as a raised surface — page `@pageContent`, container `@cardBgElevated`, flat rows `@cardBg`. In the before, the table band and page were near-identical tones. Source: DS §1 (`values.ini:2,29,30`).
3. Rebuilt the toolbar on header anatomy: a 3px accent bar + "Processes" title + muted "Live process list" source line, with the toolbar's real controls (the "All Processes" radio and the Search field) kept. Source: DS §3 (`metric_tile_base.cpp:255-307`); DS §4 typography (`style.qss:772-815`).
4. Frozen sticky header + right-aligned tabular numerics for the PID / Resident Memory / %Memory / %CPU columns [NNG-TABLES]; row zebra + hover-highlight recipe is in place for when rows populate. Source: DS §7 (`style.qss:328-350`); [NNG-TABLES] (sources.md).
5. The captured page shows an empty process table (headers, no body rows). Upgraded that bare-empty region to a full empty state: list icon + explanation + a "Refresh now" next-action affordance. Source: DS §5 empty-state rule + [NNG-EMPTY] (sources.md).

**Explicitly unchanged:** sidebar contents/order; the "All Processes" radio; the column set (PID, Resident Memory, %Memory, User, %CPU, Process) and their order; the "Refresh (1)" label + interval slider at the bottom; no invented rows or controls.
