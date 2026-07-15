# Boot Analysis — prototype rationale

**Status:** Approved by maintainer (2026-07-15).

**Before:** current-state/macos/dark/boot_analysis.png
**After:** renders/boot_analysis_{dark,light}.png

**Changes:**
1. Promoted the bare "System uptime since last boot: 721091.0 s" text off the flat page into an elevated card (DS §2 recipe) with header anatomy — an accent bar + "System uptime since last boot" title (`@color06`) + the value as the 14pt/700 hero (`@color05`). Source: DS §2 (`dashboard_tile_wrapper.cpp:107-130`); DS §3 (`metric_tile_base.cpp:255-307`); DS §4 hero role (`style.qss:772-815`). Evidence: renders/boot_analysis_dark.png.
2. Established surface hierarchy (`@pageContent` / `@cardBgElevated`) so the uptime card and the notice card read as raised surfaces rather than floating text. Source: DS §1 (`values.ini:2,29,30`).
3. Rebuilt the page toolbar on header anatomy: 3px accent bar + "Boot Analysis" title + "Startup timing" source line, with the existing top-right "Refresh" button kept. Source: DS §3 (`metric_tile_base.cpp:255-307`); DS §4 (`style.qss:772-815`).
4. Upgraded the page's existing text-only empty notice ("Per-service boot timing is not available on macOS…") into the full empty-state pattern: clock icon + explanation + a "Refresh uptime" next-action affordance, inside its own elevated card. The empty-state copy was reworded and expanded from the capture's notice text ("is not" → "isn't"; added "macOS does not expose per-service startup durations."). DS §5 explicitly names this page's `#lblBootEmptyState` as the text-only instance to upgrade. Source: DS §5 (`boot_analysis_page.cpp:78-83,145-146`); [NNG-EMPTY].

**Explicitly unchanged:** sidebar; the top-right Refresh button; the uptime value (721091.0 s); no new controls or data invented.
