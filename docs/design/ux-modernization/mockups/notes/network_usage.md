# Network Usage — prototype rationale

**Status:** Approved by maintainer (2026-07-15), including the flagged maintainer-judgment item(s).

**Before:** current-state/macos/dark/network_usage.png (and light/network_usage.png)
**After:** renders/network_usage_{dark,light}.png

**Changes (only the three approved dimensions — shadow depth, surface hierarchy, layout consistency):**
1. Wrapped each bare edge-to-edge band (throughput readout, the Today/This Week/This Month stat trio, "30-Day History", "Monthly Data Cap", "Settings") in a DS §2 elevated container card (fill `@cardBgElevated`, 1px `@borderColor`, radius 12, single drop shadow alpha 90 / blur 26 / offset 0,2, 8px outer margin). Shadows live on the container, never on internals. Source: DS §2 (`dashboard_tile_wrapper.cpp:107-130`); DS §7 (container-level shadow). Evidence: renders/network_usage_dark.png.
2. Established the three-layer surface hierarchy — page `@pageContent`, container `@cardBgElevated`, and the stat/hero readouts on the elevated surface. In the before, the bands and page were near-identical tones. Source: DS §1 (`values.ini:2,29,30`).
3. Added a DS §3 page header — 3px accent bar (`@networkColor`) + "Network Usage" title + a muted "Per-interface throughput and data usage" source line — and kept the interface selector (unchanged control) top-right. The faint page title is present in the light capture; the source line is the DS §3 pattern applied per Processes precedent. Source: DS §3 (`metric_tile_base.cpp:255-307`); DS §4 typography (`style.qss:772-815`).
4. Gave each inner section its own DS §3 accent-bar header (`@networkColor`) with the title copied exactly ("30-Day History (↓ Download ↑ Upload)", "Monthly Data Cap", "Settings"). Source: DS §3.
5. Rendered the "30-Day History" plot as a static SVG surface (no JS chart library, no animation): `@chartBackgroundColor` fill with `@chartGridColor` gridlines. The plot is empty, matching the captured empty plot — no download/upload bars were invented. The throughput arrows use the metric tokens `@networkDownloadColor` (↓) and `@networkUploadColor` (↑). Source: DS §6 (chart tokens, download/upload metric tokens); DS §9-2 (static, no animation).
6. Styled the "Monthly Data Cap" track with the DS §6 track token `@chartGridColor` (empty fill, as captured — 0% used). Source: DS §6 (`style.qss:817-826`, track `@chartGridColor`).
7. Corrected the sidebar highlight to "Network Usage" (the committed harness capture shows a stale "Dashboard" highlight — CAPTURE_NOTES.md gotcha #5). Source: CAPTURE_NOTES.md §"Gotchas" item 5.

**Explicitly unchanged:** sidebar contents/order; the interface selector (empty, as captured); the "↓ —/s  ↑ —/s" throughput readout values; the Today/This Week/This Month labels and their "—" values; the "30-Day History (↓ Download ↑ Upload)" title; the Settings controls — "Monthly cap (GB, 0 = no cap): No cap" and "Billing cycle resets on day: 1" steppers, and the green "Alert at 75%, 90%, 100% of cap" toggle (on). No usage bars or cap-fill were invented; both were empty in the capture.
