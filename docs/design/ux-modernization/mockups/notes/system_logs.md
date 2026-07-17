# System Logs — prototype rationale

**Status:** Changes 1–6 approved by maintainer (2026-07-15). Change 7 (below) is net-new scope added under SSO-14314 and is pending a fresh maintainer approval — not yet accepted.

**Before:** current-state/macos/dark/system_logs.png
**After:** renders/system_logs_{dark,light}.png

**Changes:**
1. Wrapped the full-bleed log table in one elevated container card (DS §2: `@cardBgElevated`, 1px `@borderColor`, radius 12, single drop shadow alpha 90 / blur 26 / offset 0,2, 8px margin). The shadow is on the container only; the many log rows stay flat. Source: DS §2 (`dashboard_tile_wrapper.cpp:107-130`) + DS §7. Evidence: renders/system_logs_dark.png.
2. Three-layer surface hierarchy (`@pageContent` / `@cardBgElevated` / `@cardBg`) — flat rows on `@cardBg` with the container elevated one step above the page. Source: DS §1 (`values.ini:2,29,30`).
3. Toolbar chrome rebuilt on header-anatomy spacing (this page has no title in current state): the "All Severities" select, the "Search logs…" field, and the refresh icon-button (a 24-ish `setAutoRaise`-style action button) are kept and aligned. Source: DS §3 (`metric_tile_base.cpp:255-307`, gear button 24×24 autoRaise `:171-180`); DS §4 (`style.qss:772-815`).
4. Flat rows with the DS §7 scanning kit: frozen sticky header, `@borderColor` gridlines, zebra striping realised through the DS §1 flat-row surface (`@cardBg`), and an `@accentBgTint` hover-highlight. Source: DS §7 (`style.qss:328-350`); [NNG-TABLES].
5. Severity rendered as status pills on the neutral `@chartGridColor` track: NOTICE → `info` (`@infoColor`), INFO → `dimmed` (`@tertiaryText`). Those are the only two severities present in the capture; the same `[status]` mechanism carries `warning`/`error` (`@warningColor`/`@destructiveColor`) when present. Source: DS §5 (`style.qss:2421-2435`, pill `809-815`).
6. Empty Unit cells render an em-dash placeholder (before: blank cells). Source: [NNG-TABLES].
7. **(SSO-14314, pending approval)** Added the DS §3 header-anatomy block that item 3 above intentionally omitted: a 3px accent bar (`@accentColor`, no domain-specific metric color applies to this page) + "System Logs" title + muted "Live log stream" source line, structurally identical to the `.page-header` block on Processes/Services. This is net-new scope, not a correction of item 3 — the no-title decision was correct for the SSO-13734 capture scope; a title was never in that capture to begin with. Source: DS §3 (`metric_tile_base.cpp:255-307`); DS §4 typography (`style.qss:772-815`). Evidence: renders/system_logs_{dark,light}.png.

**Explicitly unchanged:** sidebar; the All-Severities / Search / Refresh control set; the column set (Timestamp, Severity, Unit, Message); the "Showing 500 entries" footer; the log values are copied verbatim from the capture (truncations preserved), nothing invented. Change 7 adds only the header block above the existing toolbar — the log table, columns, severity pill logic, and filter/search/refresh controls and their layout are untouched.
