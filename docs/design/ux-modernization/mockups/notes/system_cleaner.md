# System Cleaner — prototype rationale

**Before:** current-state/macos/dark/system_cleaner.png
**After:** renders/system_cleaner_{dark,light}.png

**Changes:**
1. Recast the eight cleaning categories as DS §2 elevated tiles (fill `@cardBgElevated`, 1px `@borderColor`, radius 12, single drop shadow alpha 90 / blur 26 / offset 0,2). The shadow lives on each tile container, never on its inner rows — consistent with the Dashboard's one-shadow-per-tile precedent, so eight tiles stay within the bounded-shadow guardrail. Source: DS §2 (`dashboard_tile_wrapper.cpp:107-130`) + DS §7/§9-3 (container-level shadow, bounded count). Evidence: renders/system_cleaner_dark.png.
2. Established the three-layer surface hierarchy — page `@pageContent`, category tiles `@cardBgElevated`, controls on `@cardBg` — so the grid reads as raised surfaces over the page. Source: DS §1 (`values.ini:2,29,30`).
3. Rebuilt the toolbar on header anatomy: a 3px accent bar + "System Cleaner" title + the muted "Reclaim disk space by removing caches, logs, and crash reports." source line, keeping the captured gear button, "Schedule…" and "Scan system" controls. Source: DS §3 (`metric_tile_base.cpp:255-307`); DS §4 typography (`style.qss:772-815`).
4. Bottom action bar rebuilt on DS §3 spacing: the "Estimated recoverable" readout (`@successColor` value, per the captured green underline) + "Select All" + primary "Clean selected", control set unchanged. Source: DS §3 (`metric_tile_base.cpp:255-307`); DS §5 status color (`style.qss:2421-2435`).
5. Normalized the scan bar to the DS §6 progress recipe — static track `@chartGridColor`, fill `@accentColor`, radius 2, height 4 — a static placeholder with no animation per the no-per-frame guardrail. Source: DS §6 (`style.qss:817-826`); DS §9-2 (no timers/animation).

**Font note:** the per-category path source-lines (`brew · ~/Library/Caches (brew)`, `~/.Trash`, …) use the fixed monospace stack `"JetBrains Mono", ui-monospace, monospace` — the `@monoFontFamily` QSS substitution string, which is NOT a `values.ini` token. Source: `app_manager.cpp:208-209`.

**Explicitly unchanged:** sidebar contents/order (System Cleaner highlighted — the correct page highlight, not the stale "Dashboard" one in the harness capture, per CAPTURE_NOTES.md gotcha #5); the eight category titles and their monospace source paths; the per-category enable toggle (off, as captured) and the collapse (−) affordance; the gear / Schedule… / Scan system / Select All / Clean selected controls; no invented categories.
