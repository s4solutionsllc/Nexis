# UX Modernization — Paperclip Work-Item Drafts

Source: `docs/superpowers/specs/2026-07-13-ux-modernization-design.md` §5–6,
`docs/design/DESIGN_SYSTEM.md`, and the 18 prototype rationale notes under
`docs/design/ux-modernization/mockups/notes/`. These are drafts for a
**human-driven tracker (Paperclip, issue prefix `NEX`)** — there is no
Paperclip API, so every item below is pasted manually and given a real
`NEX-###` number at creation time. Each `### ` heading below has since been
updated with its assigned Paperclip identifier (see SSO-13746).

**Scope of this file:** Phase 1 (foundation) + Phase 2 (one item per page the
maintainer approved on 2026-07-15). Pages the maintainer deferred get a
one-line entry and no work item — see "Deferred pages" at the end.

All work items are **documentation/planning artifacts only**. No item in this
file authorizes touching `shared/`, `linux/`, `macos/`, or `tests/` — that
happens only when an implementing agent picks up the filed Paperclip issue.

---

## Phase 1 — Foundation (4 items)

These four items encode the v2.8.x dashboard language once, in shared code,
so every Phase-2 page item consumes a component/pattern instead of
reimplementing DS §2/§3/§5 chrome per page. All 15 approved pages consume at
least F1; most also consume F2 and F3. F4 is QSS-groundwork consumed
indirectly by every page item that touches `style.qss`.

### F1 — Shared elevated-card container component (Paperclip: SSO-13726)
**Phase:** 1-Foundation
**Blocked by:** —
**Files:**
- `shared/nexis/static/themes/default/style/style.qss` (the single master QSS
  template — card-recipe selectors, e.g. `#metricTile` block at
  `style.qss:765-769`, `#diskTile` at `style.qss:837`, plus the per-page
  `#<page>Container`/`#<page>Card` selectors this item adds; there is one
  shared template substituted per-theme by `AppManager::updateStylesheet()`,
  not a separate light-theme copy — DS §8)
- `shared/nexis/utilities.h` (`Utilities::addDropShadow()` signature —
  consumed, not changed: alpha/blur/offset contract for the elevated-card
  shadow)
- `shared/nexis/static/themes/default/style/values.ini` and
  `shared/nexis/static/themes/light/style/values.ini` (`@cardBg`,
  `@cardBgElevated`, `@borderColor`, `@shadowColor` — `values.ini:2,29,30`)
- **Reference-only, not to modify:** `shared/nexis/Pages/Dashboard/dashboard_tile_wrapper.cpp`
  (`applyDepthTreatment()`, `dashboard_tile_wrapper.cpp:107-130` — the
  existing implementation this item generalizes into a reusable QSS pattern)
**Design refs:** DS §2 (card chrome); DS §1 (surface hierarchy); DS §7 (shadow
lives on the container, never per row); notes/*.md (all 15 approved pages cite
this recipe — see "Consuming pages" below); renders/*_{dark,light}.png (every
approved render shows this container).
**Consuming pages (all 15 approved):** Processes, Resources, System Cleaner,
Search, System Logs, Hardware Info, Uninstaller, Services, Startup Apps, Disk
Tools, Helpers, Homebrew, Network Usage, Settings, Boot Analysis.
**Change summary:**
- Add a QSS selector recipe for "elevated card container" — fill
  `@cardBgElevated`, 1px `@borderColor`, radius 12, 8px outer margin — as a
  named, reusable pattern (object-name convention, e.g. `#<page>Container`)
  rather than one-off per-page selectors.
- Wire the container to a single `Utilities::addDropShadow(widget, 90, 26)`
  call site per container instance (alpha 90, blur 26, offset (0,2) per
  `utilities.h:30-32`) — never per row/child.
- Add the flat-card variant (`@cardBg`, same radius/border, no shadow) for
  content that sits inside an elevated container (DS §1).
- Document the object-name convention so page items can adopt it without
  inventing new selector names (BUG-49: no substring collisions with existing
  tokens).
**Acceptance criteria:**
- Visual match to the APPROVED render, dark + light, for the elevated-card
  chrome itself (verified via the Dashboard reference tile, which must be
  visually unchanged)
- Guardrails DS §9: no new timers/animation; shadow count unaffected by this
  item alone (it defines the recipe, doesn't apply it to pages yet); QSS-first
  (selectors added to `style.qss`, no new per-widget `setStyleSheet()` calls);
  FR-97 intact (no change to `PageSlot` lazy-construction registry,
  `app.cpp:660-663`)
- Cold-start and idle CPU unchanged within noise (macOS + Linux)
- `noRawTokensInPerWidgetStyleSheet`, `themes_sameTokenSets`,
  `darkTheme_allTokensResolved`/`lightTheme_allTokensResolved`
  (`tests/theme/test_theme_tokens.cpp`) still pass — new selectors must
  resolve in both theme files
- CHANGELOG.md + docs/APPLICATION_OVERVIEW.md updated
**UAT (business-user steps):**
1. Launch Nexis, navigate to Dashboard. Verify tile chrome (shadow, radius,
   border) looks identical to the pre-change build — this item should be
   invisible on Dashboard itself.
2. Switch Settings → Appearance → Color Scheme between Dark and Light. Verify
   no visual glitches, flashes, or unstyled flashes on Dashboard during the
   switch.
3. Confirm app cold-start time (time to first interactive frame) feels the
   same as before this change, on both a macOS and a Linux build if available.

---

### F2 — Shared section-header pattern (Paperclip: SSO-13728)
**Phase:** 1-Foundation
**Blocked by:** F1 (headers sit inside/above the card this item styles)
**Files:**
- `shared/nexis/static/themes/default/style/style.qss` (new
  `#<page>HeaderAccent` / `#<page>HeaderTitle` / `#<page>HeaderSource`
  selector group, modeled on `#metricTileAccent`/`#metricTileTitle`/
  `#metricTileSource` at `style.qss:772-815`; single master template, DS §8)
- **Reference-only, not to modify:**
  `shared/nexis/Pages/Dashboard/metric_tile_base.cpp` (`buildChrome()`,
  `metric_tile_base.cpp:255-307` — accent bar, title row, source line,
  gear-button layout this item generalizes) and
  `shared/nexis/Pages/Dashboard/metric_tile_base.h` (`FOOTER_HEIGHT = 24`,
  `metric_tile_base.h:65`)
**Design refs:** DS §3 (header anatomy); DS §4 (typography scale); notes/*.md
(every approved page rebuilds its toolbar/section title on this pattern — see
"Consuming pages" below).
**Consuming pages (all 15 approved):** Processes ("Processes" title), Resources
(per-chart accent-bar headers), System Cleaner ("System Cleaner" title),
Search (toolbar-only, no invented title), System Logs (toolbar-only), Hardware
Info (per-section accent-bar headers: System/Processor/Graphics/Memory),
Uninstaller (tab-header spacing), Services ("System Services" title), Startup
Apps ("Startup Applications" title), Disk Tools (tab-header spacing), Helpers
(group labels + "Hosts (1)" accent-bar), Homebrew ("Available Updates (3)" /
"Homebrew Packages (171)" headers), Network Usage (page header + per-section
accent-bar headers), Settings (page header + per-`QGroupBox` section headers),
Boot Analysis ("Boot Analysis" title + uptime-card header).
**Change summary:**
- Add a QSS pattern for the 3px accent bar (radius 1, `≥26px` tall, or
  `≥18px` for the smaller Settings section-card variant) + title row (title
  label, optional meta label, stretch, optional top-right 24×24 action
  button) + muted source line beneath.
- Support a type-appropriate accent-bar color token (e.g. `@cpuColor`,
  `@gpuColor`, `@networkColor`, `@accentColor`) as a per-instance property,
  not a hardcoded color (BUG-47).
- Preserve root layout margins `(14, 12, 14, 10)`, root spacing 6, header-row
  spacing 8, gear button 24×24 with 14×14 icon and `setAutoRaise(true)`
  (`metric_tile_base.cpp:171-180`), footer band 24px.
- Document that pages with no existing title (Search, System Logs, Disk
  Tools, Uninstaller) apply this pattern as spacing/alignment chrome only —
  no title/accent bar is invented where the capture had none.
**Acceptance criteria:**
- Visual match to the APPROVED render, dark + light, for header spacing and
  typography (verified against the Dashboard tile header, unchanged)
- Guardrails DS §9: no new timers/animation; QSS-first; re-polish discipline
  (BUG-56) documented for any dynamic accent-color property; FR-97 intact
- Cold-start and idle CPU unchanged within noise (macOS + Linux)
- CHANGELOG.md + docs/APPLICATION_OVERVIEW.md updated
**UAT (business-user steps):**
1. Launch Nexis, navigate to Dashboard. Verify every tile's accent bar,
   title, and source line render exactly as before this change.
2. Resize the main window narrower and wider. Verify the header row (accent
   bar + title + action button) doesn't clip or overlap at any width tested
   on Dashboard.
3. Confirm no new flashing, blinking, or animated element appears in any tile
   header.

---

### F3 — Shared state patterns: empty / loading / status pill (Paperclip: SSO-13729)
**Phase:** 1-Foundation
**Blocked by:** F1 (empty-state cards use the elevated-card container)
**Files:**
- `shared/nexis/static/themes/default/style/style.qss` (new
  `#<page>EmptyState` icon+text+action selector group; `[status="…"]`
  selectors already exist at `style.qss:2421-2435` and are reused, not
  duplicated; `#metricTileTrend` pill recipe at `style.qss:809-815` reused
  for non-Dashboard pill instances; single master template, DS §8)
- **Reference-only, not to modify:**
  `shared/nexis/Pages/BootAnalysis/boot_analysis_page.cpp`
  (`boot_analysis_page.cpp:78-83,145-146`, `#lblBootEmptyState` — the one
  existing text-only empty-state instance DS §5 names as the pattern to
  upgrade, not copy)
**Design refs:** DS §5 (status & state patterns); DS §9 item 2 (no
timers/per-frame animation — loading state is a static skeleton, no
animation); notes/*.md (empty-state consumers) and notes/*.md (status-pill
consumers).
**Consuming pages:**
- Empty-state pattern: Processes (empty process table → list icon +
  explanation + "Refresh now"), Search (empty results → search icon + "No
  results yet" + hint), Disk Tools (pre-scan empty table → disk icon +
  explanation + "Scan"), Boot Analysis (upgrades the existing
  `#lblBootEmptyState` text-only notice → clock icon + explanation +
  "Refresh uptime").
- Status-pill pattern: Services ("Running"/"Stopped" on the neutral
  `@chartGridColor` track using `[status="success"]`/`[status="dimmed"]`),
  System Logs (severity pills using `[status="info"]`/`[status="dimmed"]`,
  with `warning`/`error` available for when those severities appear), System
  Cleaner ("Estimated recoverable" `@successColor` readout).
**Change summary:**
- Add an empty-state selector group: icon slot + explanation label
  (`@tertiaryText`) + a next-action button, laid out inside a flat or
  elevated card per the consuming page's existing container.
- Confirm the `[status="…"]` generic selectors (`style.qss:2421-2435`) cover
  every semantic color a consuming page needs (`success`/`warning`/`error`/
  `info`/`dimmed`/`neutral`) without new per-page color selectors.
- Add a static skeleton-placeholder selector for loading states — explicitly
  no animation, no `QTimer`-driven repaint (DS §9 item 2).
- Do not touch `boot_analysis_page.cpp` in this item — F3 only adds the
  reusable QSS pattern; the actual upgrade of `#lblBootEmptyState` happens in
  the Boot Analysis Phase-2 item.
**Acceptance criteria:**
- Visual match to the APPROVED render, dark + light, for empty-state and
  status-pill chrome (spot-checked via a page item that consumes it once
  merged, e.g. Processes)
- Guardrails DS §9: no new timers/polling/per-frame animation (loading state
  is static); QSS-first; bounded shadows (empty-state cards count toward
  their page's shadow budget, not an additional unbounded shadow); FR-97 intact
- Cold-start and idle CPU unchanged within noise (macOS + Linux)
- CHANGELOG.md + docs/APPLICATION_OVERVIEW.md updated
**UAT (business-user steps):**
1. Launch Nexis with no processes filtered out (or navigate to a page state
   that is legitimately empty). Verify no page shows a bare blank area with
   just a table header and nothing else once this item lands and a
   consuming page item is also implemented.
2. Verify no empty-state icon spins, pulses, or otherwise animates.
3. Navigate to Services. Verify status pills read clearly in both themes
   (text legible against the pill background) once the Services page item
   consumes this pattern.

---

### F4 — QSS consolidation groundwork (Paperclip: SSO-13727)
**Phase:** 1-Foundation
**Blocked by:** —
**Files:**
- `shared/nexis/static/themes/default/style/style.qss` (2,836-line master
  template — the file every page item edits; this item establishes section
  ordering/comment-banner conventions so 15 concurrent page items don't
  collide; single master template substituted per-theme, DS §8)
- `shared/nexis/Managers/app_manager.cpp` (`updateStylesheet()`,
  `app_manager.cpp:188-194` — descending-key-length token substitution,
  BUG-49 — consumed, not changed, by this item; cited so page items know why
  new token names must not be substrings of existing ones)
- `tests/theme/test_theme_tokens.cpp` (**read-only reference** — this item
  does not modify `tests/`; it documents which existing tests
  (`themes_sameTokenSets`, `darkTheme_allTokensResolved`/
  `lightTheme_allTokensResolved`, `noRawTokensInPerWidgetStyleSheet`,
  `darkTheme_allCppTokensResolved`/`lightTheme_allCppTokensResolved`) every
  page item's QSS changes must keep passing)
- **Reference-only, not to modify:** `shared/nexis/Pages/Settings/settings_page.cpp`
  (`settings_page.cpp:213,768` — the documented drift instance: two
  `Utilities::addDropShadow({…14 widgets…}, 50)` call sites at alpha 50 /
  blur 15, versus the DS §2 target of alpha 90 / blur 26 on a container —
  cited here as the canonical "why QSS-first consolidation matters" example
  the Settings page item will fix)
**Design refs:** DS §8 (theming rules — token set, BUG-47, BUG-49,
`refreshThemeColors()` scope); DS §9 item 1 (QSS-first); notes/settings.md
("addDropShadow comparison" section — the settings inline-style drift this
groundwork exists to prevent from recurring elsewhere).
**Consuming pages:** all 15 (indirectly) — this item doesn't restyle any page
itself; it establishes the shared-QSS discipline (section banners, selector
naming, token-substitution ordering) that F1–F3 and every Phase-2 page item
build on, and calls out Settings specifically as the one page with confirmed
inline-style (per-widget `setStyleSheet()`) drift to retire.
**Change summary:**
- Add QSS section banner comments (e.g. `/* === <Page> === */`) at the start
  of each page's selector block, matching the existing `/* - Metric Tile - */`
  convention (`style.qss:764`), so 15 page items touch clearly delimited
  regions.
- Write a short contributor note (in this item's PR description, not a new
  doc file) listing the token-substring rule (BUG-49) and the
  inline-style-must-use-resolved-values rule (DS §8, "Inline styles resolve
  tokens at runtime, not at write time") so page items don't reintroduce the
  Settings drift pattern.
- Identify (list in the PR description) every remaining per-widget
  `setStyleSheet()` call site outside Settings that a future page item should
  check against `noRawTokensInPerWidgetStyleSheet()` — do not fix them in
  this item, just enumerate for the page items that touch those pages.
- No functional QSS selector changes in this item — it is organizational
  groundwork only.
**Acceptance criteria:**
- Guardrails DS §9: QSS-first; no new timers/animation; FR-97 intact; no
  shadow-count change (this item adds no shadows)
- All existing `tests/theme/test_theme_tokens.cpp` tests still pass
  unmodified (read-only per Files list)
- Cold-start and idle CPU unchanged within noise (macOS + Linux)
- CHANGELOG.md + docs/APPLICATION_OVERVIEW.md updated
**UAT (business-user steps):**
1. Launch Nexis, click through every sidebar page. Verify nothing looks or
   behaves differently — this item changes only comments/organization in
   `style.qss`, not selectors or values.
2. Confirm theme switching (Settings → Appearance → Color Scheme) still
   applies instantly and correctly on every page.

---

## Phase 2 — Pages (15 items, one per approved page)

Ordered per spec §6: Processes, Resources, System Cleaner first (high-traffic
pages). Remaining pages grouped by structure class (table-centric →
list-of-rows → tree/tool hubs → charts → forms), with Boot Analysis last
(smallest page, least risk, good final-item shakeout of the guardrails).

All 15 items below share:
- **Blocked by:** F1, F2, F3, F4
- **Acceptance criteria (common to every item, in addition to page-specific ones below):**
  - Visual match to the APPROVED render, both dark and light
    (`renders/<page>_dark.png`, `renders/<page>_light.png`)
  - Guardrails DS §9: no new timers/polling/per-frame animation; QSS-first;
    bounded shadows at the page's stated count (below); container-level
    shadows only, never per-row (DS §7); lazy construction preserved
    (FR-97 — page still builds on first navigation via `PageSlot`, not at
    launch); re-polish discipline (BUG-56) for any dynamic QSS property this
    item introduces
  - Cold-start and idle CPU unchanged within noise (macOS + Linux)
  - Screenshot baselines regenerated for this page
    (`tests/reference_screenshots/`, both themes, both platforms where the
    harness covers the page — see CAPTURE_NOTES.md coverage table for which
    platforms apply)
  - `CHANGELOG.md` updated under the current unreleased version section
  - `docs/APPLICATION_OVERVIEW.md` updated if the page's described UI changed

---

### Processes (Paperclip: SSO-13730)
**Phase:** 2-Page
**Blocked by:** F1, F2, F3, F4
**Files:**
- `shared/nexis/Pages/Processes/processes_page.cpp`
- `shared/nexis/Pages/Processes/processes_page.h`
- `shared/nexis/Pages/Processes/processes_page.ui`
**Design refs:** DS §2 (elevated container); DS §1 (surface hierarchy); DS §3
(header anatomy); DS §4 (typography); DS §7 (frozen header, tabular numerics,
zebra/hover); DS §5 (empty-state upgrade); notes/processes.md (approved
2026-07-15); renders/processes_{dark,light}.png.
**Change summary:**
- Wrap the process table in one DS §2 elevated container card (shadow on the
  container only, never on rows).
- Apply the three-layer surface hierarchy (`@pageContent` / `@cardBgElevated`
  / flat `@cardBg` rows).
- Rebuild the toolbar with a 3px accent bar + "Processes" title + "Live
  process list" source line, keeping the "All Processes" radio and Search
  field unchanged.
- Freeze the sticky header; right-align tabular numerics for PID / Resident
  Memory / %Memory / %CPU; wire zebra + hover-highlight recipe for populated
  rows.
- Upgrade the captured bare-empty table to a full empty state: list icon +
  explanation + "Refresh now" next-action affordance (maintainer-approved
  2026-07-15).
**Acceptance criteria (page-specific, in addition to the common list above):**
- Shadow count: **1** (single elevated container around the process table)
- Empty-state affordance ("Refresh now") present and functional — this is
  the maintainer-approved judgment item; verify it actually triggers a
  refresh, not just renders
- Sidebar, "All Processes" radio, column set/order (PID, Resident Memory,
  %Memory, User, %CPU, Process), and the "Refresh (1)" label + interval
  slider are unchanged
**UAT (business-user steps):**
1. Navigate to the Processes page. Verify the process table now sits inside
   a card with a visible border and soft shadow, distinct from the page
   background.
2. Verify the page shows a "Processes" title with a small colored bar to its
   left, and a muted "Live process list" line beneath it.
3. If the table is empty, verify you see an icon, an explanation of why it's
   empty, and a "Refresh now" button — click it and verify the process list
   attempts to refresh.
4. Switch to Light theme (Settings → Appearance). Verify the card, table,
   and empty state all look correct and readable in Light theme too.
5. Confirm the "All Processes" radio button and Search field still work
   exactly as before.

---

### Resources (Paperclip: SSO-13731)
**Phase:** 2-Page
**Blocked by:** F1, F2, F3, F4
**Files:**
- `shared/nexis/Pages/Resources/resources_page.cpp`
- `shared/nexis/Pages/Resources/resources_page.h`
- `shared/nexis/Pages/Resources/resources_page.ui`
- `shared/nexis/Pages/Resources/history_chart.cpp`
- `shared/nexis/Pages/Resources/history_chart.h`
- `shared/nexis/Pages/Resources/history_chart.ui`
**Design refs:** DS §2 (elevated container per chart); DS §1 (surface
hierarchy); DS §3 (per-chart header anatomy); DS §6 (chart tokens, series
palette); DS §9 item 2 (static SVG, no animation); notes/resources.md
(approved 2026-07-15); renders/resources_{dark,light}.png.
**Change summary:**
- Wrap each of the four history-chart bands (History of CPU, History of CPU
  Load Averages, History of GPU, History of Disk Read Write) in its own DS §2
  elevated container card.
- Apply the three-layer surface hierarchy; plot inset uses
  `@chartBackgroundColor` fill with `@chartGridColor` gridlines.
- Rebuild each chart header: 3px accent bar in the chart's metric token
  (`@cpuColor`, `@gpuColor`, `@diskColor`) + exact chart title + the existing
  "expand" (fullscreen) button kept unchanged top-right.
- Render plots as static SVG (no JS chart library, no animation); reproduce
  captured axis labels exactly (no invented data).
- Reproduce legend swatch rows using the DS §6 series palette per chart's
  captured swatch count (CPU=10, CPU Load Averages=3, GPU=1, Disk Read
  Write=2).
- Correct the sidebar highlight to "Resources" (harness capture has a stale
  "Dashboard" highlight — CAPTURE_NOTES.md gotcha #5).
**Acceptance criteria (page-specific, in addition to the common list above):**
- Shadow count: **4** (one elevated container per chart band) as originally
  approved 2026-07-15. Now **10** page-wide after the SSO-14437 addendum
  below (4 here + 6 more) — tracked there since those 6 tiles were outside
  this item's own approved scope (see SSO-14294).
- Chart titles, order, legend swatch counts/colors, and the per-chart
  "expand" control are unchanged; axis labels/units copied exactly; no data
  curves/bars invented in the empty captured plots
- Disk Read Write card's below-the-fold axis labels are NOT invented — only
  its visible chrome (title + 2 swatches + empty plot surface) is reproduced
**UAT (business-user steps):**
1. Navigate to the Resources page. Verify each of the four charts (CPU, CPU
   Load Averages, GPU, Disk Read Write) now sits in its own card with a
   colored accent bar matching that metric (e.g. CPU's bar color should
   match the CPU accent used elsewhere in the app).
2. Verify the sidebar highlights "Resources," not "Dashboard."
3. Click each chart's "expand" (fullscreen) button and verify it still works.
4. Switch to Light theme and verify all four chart cards remain legible with
   correct gridlines and legend colors.
5. Confirm no chart animates or auto-refreshes in a way it didn't before.

---

### Resources — remaining tiles addendum (Paperclip: SSO-14294 / SSO-14436 / SSO-14437)

**Not part of the original 2026-07-15 approval above** — filed after Maintainer
UAT on the SSO-13731/#247 change found 6 more Resources tiles still on legacy
flat chrome (see SSO-14294 for why this is new scope, not a defect in the item
above). Phase 1 (SSO-14436) produced the design refs; this addendum records
Phase 2 (SSO-14437), the implementation.
**Phase:** 2-Page (post-approval addendum)
**Blocked by:** SSO-14436 (design audit + capture)
**Files (in addition to the four already listed above):**
- `shared/nexis/Pages/Helpers/oom_kills_widget.cpp`
- `shared/nexis/Pages/Helpers/oom_kills_widget.h`
- `shared/nexis/Pages/Resources/disk_usage_launcher_widget.cpp`
- `shared/nexis/Pages/Resources/disk_usage_launcher_widget.h`
- `shared/nexis/static/themes/default/style/style.qss` (new `accentToken="temp"` selector)
**Design refs:** DS §1/§2/§3/§6/§9 (same elevated-card + accent-bar recipe as
above); `notes/resources_remaining.md` (Phase 1 audit, SSO-14436, includes the
per-tile "no invented data" disclosure and the accent-token decisions below);
`renders/resources_remaining_{dark,light}.png` (UNVERIFIED — code-derived
reference, not a live capture; see the notes doc's "Capture gap" section).
**Change summary:**
- Wrap History of Memory, History of Network, and History of Disk Temperature
  in their own DS §2 elevated container card, same recipe as the four charts
  above (`HistoryChart::setElevated()`).
- Wrap CPU Pressure Stall (some) [PSI] and Out-of-Memory Kills the same way —
  both Linux-only and self-hiding; the card chrome only applies when the tile
  is actually shown.
- Wrap Disk Usage Analysis the same way. `OomKillsWidget` and
  `DiskUsageLauncherWidget` are not `HistoryChart` instances, so each gains
  its own `setElevated(accentToken)` entry point applying the identical
  `[cardRole="elevated"]` + accent-bar + 90/26-drop-shadow recipe around their
  existing content — no redesign of OOM Kills' info layout or the Disk Usage
  launcher's tool-detection UI.
- Accent-token decisions (flagged as open in the Phase 1 audit, resolved
  here): Memory → `memory` (pre-existing selector); Network → `network`
  (pre-existing selector); Disk Temperature → new `temp` selector
  (`@tempColor`, previously a color value with nothing wired to it); PSI →
  `cpu` (it's a CPU-pressure metric, reusing the existing selector rather than
  adding a new one); OOM Kills → `memory` (memory-pressure-adjacent, reusing
  the existing selector over the also-considered `@warningColor`, which stays
  reserved for the card's internal cgroup-v2 tip); Disk Usage Analysis →
  `disk` (matches Disk Read Write's existing accent, both disk-related).
**Acceptance criteria (page-specific, in addition to the common list above):**
- Shadow count: **10** total on the Resources page (4 from SSO-13731/#247 +
  6 new here) — one elevated container per tile, no per-row/per-child shadows
- Tile titles, order, and (where applicable) legend swatch counts/colors are
  unchanged; axis labels/units copied exactly per `notes/resources_remaining.md`;
  no data curves/bars/counts invented in the empty/representative captured
  states
- PSI and OOM Kills remain Linux-only and self-hiding exactly as before —
  the elevated chrome does not change when/whether they appear
- The 4 tiles converted in #247 show no regression (unchanged accent
  colors, unchanged "expand" behavior, unchanged shadow)
**UAT (business-user steps):**

*History of Memory*
1. Navigate to the Resources page and locate "History of Memory."
2. Verify it now sits in its own card with a 3px accent bar colored to match
   the Memory metric (same color as Memory's accent elsewhere in the app).
3. Verify the 4-series legend (Swap, Memory, Wired/Available, Compressed/Active)
   and axis still read correctly, unchanged from before.
4. Switch to Light theme and verify the card remains legible with correct
   gridlines and legend colors.
5. Confirm the chart still updates live and does not animate/auto-refresh in
   a way it didn't before.

*History of Network*
1. Navigate to the Resources page and locate "History of Network."
2. Verify it now sits in its own card with a 3px accent bar colored to match
   the Network metric.
3. Verify the Download/Upload legend and the byte-formatted axis are
   unchanged and still populate once live data arrives.
4. Switch to Light theme and verify the card remains legible.
5. Confirm the chart still updates live with no new animation.

*History of Disk Temperature*
1. On a host reporting at least one drive temperature, navigate to the
   Resources page and locate "History of Disk Temperature." (If no drive
   reports a temperature, this tile does not exist — confirm it is simply
   absent, not broken.)
2. Verify it sits in its own card with a 3px accent bar in the new "temp"
   color (`@tempColor`).
3. Verify the per-drive legend still reflects the actual detected drive
   count and the axis is unchanged.
4. Switch to Light theme and verify the card remains legible.
5. Confirm the chart still updates on the existing refresh cadence.

*CPU Pressure Stall (some) [PSI]*
1. On Linux with `/proc/pressure/cpu` present, navigate to the Resources
   page and locate "CPU Pressure Stall (some)." (Absent on macOS or hosts
   without PSI — confirm it is simply absent there, not broken.)
2. Verify it sits in its own card with a 3px accent bar in the CPU color
   (reused from CPU/CPU Load Averages).
3. Verify the avg10/avg60/avg300 legend and axis are unchanged.
4. Switch to Light theme and verify the card remains legible.
5. Confirm the chart still updates live with no new animation.

*Out-of-Memory Kills*
1. On a Linux host with oomd/cgroup v2 signal available, navigate to the
   Resources page and locate "Out-of-Memory Kills." (Hidden entirely on
   hosts without that signal, and on macOS — confirm it is simply absent
   there, not broken.)
2. Verify the card now sits in its own elevated card with a 3px accent bar
   (memory color) to the left of the title — same recipe as the chart tiles,
   not a bespoke look.
3. Verify the state line, totals line, defensive tip (if shown), and recent
   kills list all still render exactly as before — no content changed, only
   the surrounding chrome.
4. Switch to Light theme and verify the card remains legible.
5. Confirm nothing about the card animates or auto-refreshes in a way it
   didn't before.

*Disk Usage Analysis*
1. Navigate to the Resources page and locate "Disk Usage Analysis."
2. Verify it now sits in its own elevated card with a 3px accent bar (disk
   color) next to the title.
3. Verify the tool icon/name/description/status row and the "Built-in
   Treemap" / launch-or-install action button are all still present and
   functional (click through to confirm the button still launches/installs
   as before).
4. Switch to Light theme and verify the card remains legible.
5. Confirm no regression to tool auto-detection or the built-in treemap
   dialog.

---

### System Cleaner (Paperclip: SSO-13732)
**Phase:** 2-Page
**Blocked by:** F1, F2, F3, F4
**Files:**
- `shared/nexis/Pages/SystemCleaner/system_cleaner_page.cpp`
- `shared/nexis/Pages/SystemCleaner/system_cleaner_page.h`
- `shared/nexis/Pages/SystemCleaner/system_cleaner_page.ui`
**Design refs:** DS §2 (elevated tiles); DS §7 / DS §9 item 3 (container-level
shadow, bounded count); DS §1 (surface hierarchy); DS §3 (header anatomy); DS
§5 (status color for "Estimated recoverable"); DS §6 (static progress-bar
recipe); notes/system_cleaner.md (approved 2026-07-15);
renders/system_cleaner_{dark,light}.png.
**Change summary:**
- Recast the eight cleaning categories as DS §2 elevated tiles in a grid —
  one container-level shadow per tile (consistent with the Dashboard's
  one-shadow-per-tile precedent; 8 tiles stays within the bounded-shadow
  guardrail).
- Apply the three-layer surface hierarchy (page / category tiles / controls
  on `@cardBg`).
- Rebuild the toolbar: 3px accent bar + "System Cleaner" title + "Reclaim
  disk space by removing caches, logs, and crash reports." source line;
  keep the gear button, "Schedule…", and "Scan system" controls.
- Rebuild the bottom action bar: "Estimated recoverable" readout on
  `@successColor` + "Select All" + primary "Clean selected", controls
  unchanged.
- Normalize the scan bar to the DS §6 progress recipe (static track
  `@chartGridColor`, fill `@accentColor`, radius 2, height 4) — static, no
  animation.
- Use the fixed monospace stack (`"JetBrains Mono", ui-monospace, monospace`
  — `@monoFontFamily` QSS string, `app_manager.cpp:208-209`) for per-category
  path source-lines.
**Acceptance criteria (page-specific, in addition to the common list above):**
- Shadow count: **8** (one per category tile) — verify this does not grow
  further (e.g. no additional shadow on the bottom action bar)
- Eight category titles and monospace source paths, the per-category enable
  toggle (off, as captured) and collapse (−) affordance, and the gear /
  Schedule… / Scan system / Select All / Clean selected controls unchanged;
  no invented categories
- Scan-progress bar renders as a static placeholder — confirm no
  `QTimer`-driven or per-frame repaint was added
**UAT (business-user steps):**
1. Navigate to System Cleaner. Verify all eight cleaning categories (brew
   caches, Trash, etc.) each appear as a separate card with a visible border
   and shadow.
2. Verify the page title area shows "System Cleaner" with a description
   line beneath it, and the gear / Schedule… / Scan system buttons are still
   present and clickable.
3. Verify the bottom bar shows "Estimated recoverable" in green, plus
   "Select All" and "Clean selected" buttons, functioning as before.
4. Click "Scan system" and verify the progress bar appears as a thin static
   bar (no spinning/pulsing animation) while scanning.
5. Switch to Light theme and verify all eight category cards remain legible.

---

### Search (Paperclip: SSO-13733)
**Phase:** 2-Page
**Blocked by:** F1, F2, F3, F4
**Files:**
- `shared/nexis/Pages/Search/search_page.cpp`
- `shared/nexis/Pages/Search/search_page.h`
- `shared/nexis/Pages/Search/search_page.ui`
**Design refs:** DS §2 (elevated container); DS §1 (surface hierarchy); DS §3
(toolbar-chrome-only header, no invented title); DS §7 (frozen header,
tabular Size); DS §5 (empty-state upgrade); notes/search.md (approved
2026-07-15); renders/search_{dark,light}.png.
**Change summary:**
- Wrap the results table in one DS §2 elevated container card (shadow on the
  container only).
- Apply the three-layer surface hierarchy.
- Apply header-anatomy spacing to the toolbar only (this page has no page
  title in the current state) — keep Browse…, Search field, "All" scope
  select, the primary search-icon button, and "Advanced Search ▾" unchanged;
  do not invent a title/accent bar.
- Freeze the sticky header; right-align tabular numeric alignment on Size;
  wire zebra + hover recipe for result rows.
- Upgrade the captured empty results table to a full empty state: search
  icon + "No results yet" + a hint pointing at the query field / Browse… as
  the next action (maintainer-approved 2026-07-15).
**Acceptance criteria (page-specific, in addition to the common list above):**
- Shadow count: **1** (single elevated container around the results table)
- No title/accent bar invented where the capture had none
- Empty-state affordance ("No results yet" + hint) present; Browse… /
  Search / All-scope / search-button row and the column set (Name, Path,
  Size, User, Creation Time) unchanged; no invented rows
**UAT (business-user steps):**
1. Navigate to Search. Verify the results area now sits inside a bordered
   card with a soft shadow.
2. Run a search that returns no results (or view the page before searching).
   Verify you see a search icon, "No results yet" text, and a hint about
   using Browse… or the search field — not a bare empty table.
3. Confirm Browse…, the search field, the "All" scope dropdown, the search
   button, and "Advanced Search ▾" all still work as before.
4. Switch to Light theme and confirm the results card and empty state remain
   legible.

---

### System Logs (Paperclip: SSO-13734)
**Phase:** 2-Page
**Blocked by:** F1, F2, F3, F4
**Files:**
- `shared/nexis/Pages/SystemLogs/system_logs_page.cpp`
- `shared/nexis/Pages/SystemLogs/system_logs_page.h`
**Design refs:** DS §2 (elevated container); DS §1 (surface hierarchy); DS §3
(toolbar-chrome-only header); DS §7 (frozen header, gridlines, zebra, hover);
DS §5 (severity status pills); notes/system_logs.md (approved 2026-07-15);
renders/system_logs_{dark,light}.png.
**Change summary:**
- Wrap the log table in one DS §2 elevated container card (shadow on the
  container only; many log rows stay flat).
- Apply the three-layer surface hierarchy.
- Apply header-anatomy spacing to the toolbar (no title in current state):
  keep "All Severities" select, "Search logs…" field, and the refresh
  icon-button (24×24 autoRaise style) aligned per DS §3.
- Wire the DS §7 scanning kit: frozen sticky header, `@borderColor`
  gridlines, zebra striping via the `@cardBg` flat-row surface, and
  `@accentBgTint` hover-highlight.
- Render severity as status pills on the `@chartGridColor` track: NOTICE →
  `info`, INFO → `dimmed` (the only two severities present in the capture;
  `warning`/`error` selectors already exist for when those appear).
- Render empty Unit cells as an em-dash placeholder instead of blank.
**Acceptance criteria (page-specific, in addition to the common list above):**
- Shadow count: **1** (single elevated container around the log table)
- Column set (Timestamp, Severity, Unit, Message), the "Showing 500 entries"
  footer, and log values (including truncations) unchanged; no invented log
  entries
- Severity pill colors correctly map NOTICE→info, INFO→dimmed and are ready
  to display warning/error via the same mechanism without further code
  changes
**UAT (business-user steps):**
1. Navigate to System Logs. Verify the log table sits inside a bordered card
   with a soft shadow.
2. Verify severity values (NOTICE, INFO, etc.) now render as small colored
   pill badges rather than plain text.
3. Verify empty "Unit" cells show a dash ("—") instead of being blank.
4. Confirm "All Severities" filter, "Search logs…" field, and the refresh
   button still work.
5. Switch to Light theme and confirm the log table and pills remain
   legible.

---

### Hardware Info (Paperclip: SSO-13735)
**Phase:** 2-Page
**Blocked by:** F1, F2, F3, F4
**Files:**
- `shared/nexis/Pages/HardwareInfo/hardware_info_page.cpp`
- `shared/nexis/Pages/HardwareInfo/hardware_info_page.h`
- `shared/nexis/Pages/HardwareInfo/hardware_info_page.ui`
**Design refs:** DS §2 (elevated cards per section); DS §1 (surface
hierarchy); DS §3 (per-section accent-bar headers, type-appropriate accent
color); DS §4 (key/value typography); notes/hardware_info.md (System/
Processor/Graphics/Memory approved 2026-07-15; Battery card added round 2,
Paperclip: SSO-14298, pending maintainer UAT); renders/hardware_info_{dark,light}.png.
**Change summary:**
- Put each section body (System, Processor, Graphics, Memory, **Battery**) in
  its own DS §2 elevated card. Battery was out of scope for the original pass
  (see below) and was added in round 2 once a capture with real Battery rows
  existed.
- Apply the three-layer surface hierarchy so each card reads as raised.
- Give each section title header anatomy — 3px accent bar + label; Graphics
  uses the GPU metric token (`@gpuColor`), Battery uses `@batteryColor`
  (already wired for the header accent bar pre-round-2), other sections use
  their type-appropriate accent tokens.
- Apply DS §4 typography to key/value rows: labels 9pt/600 `@color06`,
  values `@color05`, with an `@borderColor` divider between rows.
**Acceptance criteria (page-specific, in addition to the common list above):**
- Shadow count: **5** (System, Processor, Graphics, Memory, Battery — round 1
  shipped 4 with Battery as a section-title-only stub because the only
  available capture cut off before any Battery rows; round 2, SSO-14298, adds
  the 5th once a full capture existed: `current-state/macos/{dark,light}/
  hardware_info_battery.png`, manual capture 2026-07-17)
- Section order (System → Processor → Graphics → Memory → Battery) and every
  key/value copied verbatim from the round-2 capture; the full-width "Copy
  GPU Diagnostics" button unchanged; no Battery rows invented — every key in
  the Battery card must trace to a row visible in
  `hardware_info_battery.png`
**UAT (business-user steps):**
1. Navigate to Hardware Info. Verify System, Processor, Graphics, Memory, and
   Battery each appear as separate cards with borders and shadows.
2. Verify the Graphics card's accent bar is a different color from the other
   sections' accent bars.
3. Verify "Copy GPU Diagnostics" still works.
4. Scroll to Battery and confirm its card shows the same key/value rows as
   today (Status, Health, Charge, Cycle Count, Current/Maximum/Design
   Capacity, Temperature, Voltage, Power, Time Remaining) with no fabricated
   rows — only styling changed, not content.
5. Switch to Light theme and confirm all five cards remain legible.

---

### Uninstaller (sidebar label "Applications") (Paperclip: SSO-13736)
**Phase:** 2-Page
**Blocked by:** F1, F2, F3, F4
**Files:**
- `shared/nexis/Pages/Uninstaller/uninstaller_page.cpp`
- `shared/nexis/Pages/Uninstaller/uninstaller_page.h`
- `shared/nexis/Pages/Uninstaller/uninstallerpage.ui`
**Design refs:** DS §2 (elevated container); DS §1 (surface hierarchy); DS §3
(tab-header spacing, active-tab accent); DS §7 (frozen column header, group
row); notes/uninstaller.md (approved 2026-07-15);
renders/uninstaller_{dark,light}.png.
**Change summary:**
- Wrap the application list in one DS §2 elevated container card (shadow on
  the container, flat rows inside).
- Apply the three-layer surface hierarchy.
- Apply header-anatomy spacing to the "Applications (49)" / "Orphan Packages
  (0)" segmented tabs and the Search field; active tab uses `@accentColor`.
- Freeze the "Application" column header (`@cardBgElevated`, `@borderColor`
  bottom rule); the "▸ Applications (49)" group row is a flat `@cardBg`-
  eligible row inside the elevated card.
**Acceptance criteria (page-specific, in addition to the common list above):**
- Shadow count: **1** (single elevated container around the application list)
- Note: the sidebar entry for this page reads "Applications" on macOS, not
  "Uninstaller" — verify the correct sidebar label is highlighted, not a
  literal "Uninstaller" label
- Two tabs and their counts, the collapsed "Applications (49)" tree group
  (kept collapsed, no child rows invented), the Search field, and the
  centered "Uninstall Selected" primary action unchanged
**UAT (business-user steps):**
1. Click the sidebar item labeled "Applications." Verify the app list sits
   inside a bordered, shadowed card.
2. Verify the "Applications (49)" / "Orphan Packages (0)" tabs still switch
   correctly and the active tab is visually highlighted.
3. Verify the application list group stays collapsed by default (no new
   items appear that weren't there before).
4. Confirm the "Uninstall Selected" button at the bottom still works.
5. Switch to Light theme and confirm the list and tabs remain legible.

---

### Services (Paperclip: SSO-13737)
**Phase:** 2-Page
**Blocked by:** F1, F2, F3, F4
**Files:**
- `shared/nexis/Pages/Services/services_page.cpp`
- `shared/nexis/Pages/Services/services_page.h`
- `shared/nexis/Pages/Services/services_page.ui`
- `shared/nexis/Pages/Services/service_item.cpp`
- `shared/nexis/Pages/Services/service_item.h`
- `shared/nexis/Pages/Services/service_item.ui`
**Design refs:** DS §2 (elevated container replacing per-row shadows); DS §7
(shadow on container, never per row — supersedes the current
`Utilities::addDropShadow(this, 30, 10)` at `service_item.cpp:30`); DS §1
(surface hierarchy); DS §3 (header anatomy); DS §4 (row typography); DS §5
(status pill); notes/services.md (approved 2026-07-15);
renders/services_{dark,light}.png.
**Change summary:**
- Collapse the stack of individually-shadowed `ServiceItem` cards into one DS
  §2 elevated container card holding flat `@cardBg` rows — **remove** the
  per-row `Utilities::addDropShadow(this, 30, 10)` call in
  `service_item.cpp:30`.
- Apply the three-layer surface hierarchy.
- Rebuild the toolbar: 3px accent bar + "System Services" title + "27
  services" source line (moves the captured "(27)" count into the source
  line); keep the "Startup Status"/"Running Status" filter dropdowns and the
  start-all/stop-all icon actions.
- Apply row typography: service label at 9pt/600 title role, `– identifier`
  suffix at muted meta role, ellipsis-clip on long identifiers, hover-
  highlight on rows.
- Render status on the DS §5 pill track: "Running" = `@successColor`,
  "Stopped" = `@tertiaryText` dimmed, replacing the current solid-green /
  bordered-grey chips. Keep the green on/off toggle unchanged.
**Acceptance criteria (page-specific, in addition to the common list above):**
- Shadow count: **1** (single elevated container; verify the per-row
  `addDropShadow(this, 30, 10)` call in `service_item.cpp` is removed, not
  just visually hidden)
- Filter dropdowns, start-all/stop-all actions, per-row toggle, and every
  service name/identifier/status copied verbatim (27 rows, first 13
  visible); no invented rows
**UAT (business-user steps):**
1. Navigate to Services. Verify the entire service list sits inside ONE
   bordered, shadowed card — individual rows should NOT each have their own
   shadow.
2. Verify each row shows "Running" or "Stopped" as a small colored pill, not
   a solid chip.
3. Confirm the "Startup Status" / "Running Status" filters and the
   start-all/stop-all buttons at top-right still work.
4. Toggle a service on/off using its row toggle and confirm it still
   functions.
5. Switch to Light theme and confirm the list and pills remain legible.

---

### Startup Apps (Paperclip: SSO-13738)
**Phase:** 2-Page
**Blocked by:** F1, F2, F3, F4
**Files:**
- `shared/nexis/Pages/StartupApps/startup_apps_page.cpp`
- `shared/nexis/Pages/StartupApps/startup_apps_page.h`
- `shared/nexis/Pages/StartupApps/startup_apps_page.ui`
- `shared/nexis/Pages/StartupApps/startup_app.cpp`
- `shared/nexis/Pages/StartupApps/startup_app.h`
- `shared/nexis/Pages/StartupApps/startup_app.ui`
- `macos/nexis/Pages/StartupApps/startup_app_edit.cpp` (platform variant)
- `linux/nexis/Pages/StartupApps/startup_app_edit.cpp` (platform variant)
  (edit dialog: inherits shared chrome from F1–F3 only — no dedicated prototype
  exists, so make no dialog-specific visual changes in this item)
**Design refs:** DS §2 (elevated container); DS §7 (container-level shadow);
DS §1 (surface hierarchy); DS §3 (header anatomy); DS §4 (two-line row
typography); [GNOME-HIG] (grouped-list convention); notes/startup_apps.md
(approved 2026-07-15); renders/startup_apps_{dark,light}.png.
**Change summary:**
- Wrap the per-item startup cards in one DS §2 elevated container card with
  flat `@cardBg` rows inside — one container-level shadow instead of a
  shadow per row.
- Apply the three-layer surface hierarchy.
- Rebuild the toolbar: 3px accent bar + "Startup Applications" title + "22
  items" source line (moves the captured "(22)" count); keep the Search
  field, "Add Startup App" primary action, and "Repair BTM…" action (kept on
  the danger button token to match its captured red treatment).
- Preserve the "User Agents" / "System Agents" section grouping as muted
  uppercase group labels between flat rows; keep each row's two-line layout
  (app name 9pt/600 title role, `.plist` path muted meta role) and its edit
  (pencil) / remove (X) / green enable-toggle affordances unchanged.
**Acceptance criteria (page-specific, in addition to the common list above):**
- Shadow count: **1** (single elevated container around the startup-item
  list)
- Search / Add Startup App / Repair BTM toolbar controls, the User
  Agents/System Agents grouping, per-row edit/remove/toggle affordances, and
  every app name/path copied from the capture unchanged; app-icon
  placeholders only where the capture showed one; no invented rows
**UAT (business-user steps):**
1. Navigate to Startup Apps. Verify all startup items sit inside ONE
   bordered, shadowed card grouped under "User Agents" / "System Agents"
   labels.
2. Confirm the Search field, "Add Startup App," and "Repair BTM…" buttons
   still work as before.
3. Click the edit (pencil) and remove (X) icons on a row and confirm they
   still function.
4. Toggle a startup item's enable switch and confirm it still works.
5. Switch to Light theme and confirm the list and grouping remain legible.

---

### Disk Tools (Paperclip: SSO-13739)
**Phase:** 2-Page
**Blocked by:** F1, F2, F3, F4
**Files:**
- `shared/nexis/Pages/DiskTools/disk_tools_page.cpp`
- `shared/nexis/Pages/DiskTools/disk_tools_page.h`
- `shared/nexis/Pages/DiskTools/disk_tools_page.ui`
**Design refs:** DS §2 (elevated containers); DS §7 (container-level shadow);
DS §1 (surface hierarchy); DS §3 (tab-header spacing); DS §7 (frozen header,
tabular Size); DS §5 (empty-state upgrade); notes/disk_tools.md (approved
2026-07-15); renders/disk_tools_{dark,light}.png.
**Change summary:**
- Wrap the scan-roots list and the results table each in their own DS §2
  elevated container card — scan-root rows and result rows stay flat inside
  their container.
- Apply the three-layer surface hierarchy.
- Apply header-anatomy spacing to the "Large & Old Files" / "Duplicate
  Finder" tabs (no title label in current state); active tab uses
  `@accentColor`.
- Freeze the sticky header + right-align the Size column; upgrade the
  pre-scan empty table to a full empty state (disk icon + explanation +
  "Scan" next-action affordance) rather than a bare header over blank space
  (maintainer-approved 2026-07-15).
- Rebuild the bottom action bar on DS §3 spacing: "No files selected"
  readout + "Move to Trash", controls unchanged.
- Use the fixed monospace stack for scan-root path rows
  (`@monoFontFamily`, `app_manager.cpp:208-209`).
**Acceptance criteria (page-specific, in addition to the common list above):**
- Shadow count: **2** (scan-roots list container + results table container)
- Two mode tabs, scan-roots list + Add…/Remove buttons, criteria controls
  (Size ≥, Not-accessed ≥, Match select) and their values (100 MB / 180 days
  / Either), the Scan button, the five result columns (Name/Path/Size/Last
  Accessed/Last Modified) and order, and "Move to Trash" unchanged; no
  invented result rows
- Empty-state affordance ("Scan") present pre-scan
**UAT (business-user steps):**
1. Navigate to Disk Tools. Verify the scan-roots list and the results area
   each sit inside their own bordered, shadowed card.
2. Before scanning, verify the results area shows a disk icon, an
   explanation, and a "Scan" button — not a bare empty table.
3. Confirm the "Large & Old Files" / "Duplicate Finder" tabs still switch
   correctly.
4. Run a scan and confirm results populate the table with correct columns
   and that "Move to Trash" still works on a selection.
5. Switch to Light theme and confirm both cards remain legible.

---

### Helpers (Paperclip: SSO-13740)
**Phase:** 2-Page
**Blocked by:** F1, F2, F3, F4
**Files:**
- `shared/nexis/Pages/Helpers/helpers_page.cpp`
- `shared/nexis/Pages/Helpers/helpers_page.h`
- `shared/nexis/Pages/Helpers/helpers_page.ui`
- `shared/nexis/Pages/Helpers/host_manage.cpp`
- `shared/nexis/Pages/Helpers/host_manage.h`
- `shared/nexis/Pages/Helpers/host_manage.ui`
**Design refs:** DS §2 (elevated action cards + container); DS §7
(container-level shadow, flat rows); DS §1 (surface hierarchy); DS §3 (group
labels, accent-bar sub-section header); DS §4 (title/description typography);
DS §7 (frozen header); notes/helpers.md (approved 2026-07-15);
renders/helpers_{dark,light}.png.
**Change summary:**
- Recast the four Maintenance actions as DS §2 elevated action cards in a
  4-up grid; put the Host Manage table in one DS §2 elevated container with
  flat `@cardBg` rows.
- Apply the three-layer surface hierarchy.
- Style "Tools" / "Maintenance" group labels on the DS §4 meta role; each
  Maintenance card as title (DS §4 title role) over description (DS §4 meta
  role); give the "Hosts (1)" sub-section a 3px accent-bar header. Keep the
  captured tool buttons with Host Manage active (`@accentColor`).
- Freeze the sticky header for the hosts table (IP Address / Full Qualified
  / Aliases); keep "New Host" and bottom "Save Changes" actions on DS §3
  spacing.
- Use the fixed monospace stack for host table cells (`@monoFontFamily`,
  `app_manager.cpp:208-209`).
**Acceptance criteria (page-specific, in addition to the common list above):**
- Shadow count: **5** (four Maintenance action cards + one Host Manage table
  container)
- Six Tools buttons (Host Manage active, Network Diagnostics, Open Ports,
  Firewall, SSD TRIM, Wake-on-LAN) and order, the four Maintenance
  cards/titles/descriptions, and the Host Manage widget's heading/New
  Host/table columns/Save Changes unchanged. Only Host Manage is shown per
  the "one representative tool widget" scope — the other five tool widgets
  are explicitly out of scope for this item
**UAT (business-user steps):**
1. Navigate to Helpers. Verify the four Maintenance actions each appear as
   separate cards in a row/grid with borders and shadows.
2. Verify the Tools button row (Host Manage, Network Diagnostics, Open
   Ports, Firewall, SSD TRIM, Wake-on-LAN) still switches the active tool
   widget correctly.
3. With Host Manage active, verify the hosts table sits inside its own
   bordered, shadowed card with a "Hosts (1)" header.
4. Add a host via "New Host," confirm it appears in the table, then click
   "Save Changes" and confirm it persists.
5. Switch to Light theme and confirm all cards remain legible.

---

### Homebrew (macOS only) (Paperclip: SSO-13741)
**Phase:** 2-Page
**Blocked by:** F1, F2, F3, F4
**Files:**
- `macos/nexis/Pages/Homebrew/homebrew_page.cpp`
- `macos/nexis/Pages/Homebrew/homebrew_page.h`
**Design refs:** DS §2 (elevated containers); DS §7 (container-level shadow);
DS §1 (surface hierarchy); DS §3 (header anatomy); DS §7 (frozen header,
tabular Version, collapsed tree state); notes/homebrew.md (approved
2026-07-15); renders/homebrew_{dark,light}.png.
**Change summary:**
- Wrap the "Available Updates" table and the "Homebrew Packages" tree each
  in their own DS §2 elevated container card — update rows and collapsible
  tree-group rows stay flat inside their container.
- Apply the three-layer surface hierarchy.
- Rebuild the top of the page: 3px accent bar + "Available Updates (3)"
  title + "Outdated Homebrew packages" source line, keeping "Check Now"; the
  second section gets a matching accent-bar "Homebrew Packages (171)" header
  with the "Search packages" field kept.
- Freeze sticky headers + right-align the Version column; keep the two
  package groups ("Homebrew Cask (6)", "Homebrew Formula (165)") in their
  collapsed tree state with the disclosure caret, as captured.
- Use the fixed monospace stack for update-row and tree cells
  (`@monoFontFamily`, `app_manager.cpp:208-209`).
**Acceptance criteria (page-specific, in addition to the common list above):**
- Shadow count: **2** (Available Updates container + Homebrew Packages
  container)
- macOS-only page — verify this page is not reachable/rendered on Linux
  builds (Linux compiles APT Source Manager in this sidebar slot instead)
- "Available Updates (3)" columns (Source/Package/Version) and its three
  rows (dav1d 1.5.4, ruby 4.0.6, blender 5.2.0), "Check Now," the
  "Homebrew Packages (171)" tree with its two collapsed groups/counts, the
  "Search packages" field, and the bottom Install/Uninstall controls
  unchanged; no invented rows or expanded children
**UAT (business-user steps):**
1. On a macOS build, navigate to Homebrew (sidebar shows a badge with the
   update count). Verify "Available Updates" and "Homebrew Packages" each
   sit inside their own bordered, shadowed card.
2. Confirm "Check Now" still triggers an update check.
3. Confirm "Homebrew Cask" and "Homebrew Formula" groups remain collapsed by
   default and expand correctly when clicked.
4. Use "Search packages" and confirm filtering still works.
5. Switch to Light theme and confirm both cards remain legible.

---

### Network Usage (Paperclip: SSO-13742)
**Phase:** 2-Page
**Blocked by:** F1, F2, F3, F4
**Files:**
- `shared/nexis/Pages/Network/network_usage_page.cpp`
- `shared/nexis/Pages/Network/network_usage_page.h`
**Design refs:** DS §2 (elevated containers per band); DS §1 (surface
hierarchy); DS §3 (page header + per-section accent-bar headers); DS §6
(chart tokens, static SVG plot, track token); notes/network_usage.md
(approved 2026-07-15); renders/network_usage_{dark,light}.png.
**Change summary:**
- Wrap each band (throughput readout; Today/This Week/This Month stat trio;
  "30-Day History"; "Monthly Data Cap"; "Settings") in its own DS §2 elevated
  container card.
- Apply the three-layer surface hierarchy.
- Add a page header: 3px accent bar (`@networkColor`) + "Network Usage"
  title + "Per-interface throughput and data usage" source line; keep the
  interface selector top-right unchanged.
- Give each inner section its own DS §3 accent-bar header (`@networkColor`)
  with the exact captured title ("30-Day History (↓ Download ↑ Upload)",
  "Monthly Data Cap", "Settings").
- Render "30-Day History" as a static SVG plot (`@chartBackgroundColor` +
  `@chartGridColor`), empty as captured — no invented data; throughput
  arrows use `@networkDownloadColor` / `@networkUploadColor`.
- Style "Monthly Data Cap" track with `@chartGridColor` (empty fill, 0%
  used, as captured).
- Correct the sidebar highlight to "Network Usage" (harness capture has a
  stale "Dashboard" highlight — CAPTURE_NOTES.md gotcha #5).
**Acceptance criteria (page-specific, in addition to the common list above):**
- Shadow count: **5** (throughput readout, stat trio, 30-Day History,
  Monthly Data Cap, Settings)
- Interface selector (empty, as captured), throughput readout values
  ("↓ —/s ↑ —/s"), Today/This Week/This Month labels and "—" values, the
  "30-Day History" title, and the Settings controls (monthly cap stepper,
  billing-cycle-day stepper, "Alert at 75%, 90%, 100%" toggle) unchanged; no
  usage bars or cap-fill invented
**UAT (business-user steps):**
1. Navigate to Network Usage. Verify the sidebar highlights "Network Usage,"
   not "Dashboard."
2. Verify the throughput readout, the Today/This Week/This Month stats, the
   30-Day History chart, Monthly Data Cap, and Settings each sit in their
   own bordered, shadowed card.
3. Confirm the interface selector still works if multiple interfaces are
   available.
4. Confirm the Monthly Data Cap stepper, billing-cycle-day stepper, and
   "Alert at…" toggle still function.
5. Switch to Light theme and confirm all five cards remain legible.

---

### Settings (Paperclip: SSO-13743)
**Phase:** 2-Page
**Blocked by:** F1, F2, F3, F4
**Files:**
- `shared/nexis/Pages/Settings/settings_page.cpp`
- `shared/nexis/Pages/Settings/settings_page.h`
- `shared/nexis/Pages/Settings/settings_page.ui`
**Design refs:** DS §2 (elevated section cards); DS §1 (surface hierarchy);
DS §3 (page header + per-section headers); DS §4 (label/field typography,
[HIG-MACOS] control alignment); DS §9 item 3 (bounded-shadow correction — see
"Fixes the documented drift" below); notes/settings.md (approved 2026-07-15);
renders/settings_dark.png + renders/settings_light.png (approved targets).
**Change summary:**
- Turn each `QGroupBox` form section (`groupGeneral`, `groupAppearance`,
  `groupAlerts`, `groupTools`, `groupScheduledCleaning`) into one DS §2
  elevated section card.
- Apply the three-layer surface hierarchy (`@pageContent` behind the scroll,
  `@cardBgElevated` section cards, flat controls on top).
- Give every section card a DS §3 header (3px `@accentColor` bar + section
  title); add a page-level accent-bar header ("Settings" + "Preferences and
  alerts" source line) above the scroll.
- Apply DS §4 label/field typography and [HIG-MACOS] alignment: label left
  (9pt/600 `@color06`), control right; keep all captured combo/spinbox/toggle
  values unchanged (Language English, Start Page Dashboard, Default Disk
  /dev/disk3s1s1, Color Scheme Auto, Font Inter (Recommended), Tray Icon
  Style Color (Default), Disk Analyzer Auto (Detect); alert thresholds all
  0%; "Show Dashboard system summary and status bar" and "Enable disk health
  alerts" on, rest off).
- **Fixes the documented drift:** remove the two
  `Utilities::addDropShadow({…14 widgets…}, 50)` call sites
  (`settings_page.cpp:213` in `init()`, `settings_page.cpp:768` in
  `refreshThemeColors()`) that apply a per-control shadow at alpha 50 / blur
  15 to 14 individual widgets (`cmbLanguages`, `cmbDisks`, `cmbStartPage`,
  `cmbColorScheme`, `cmbFont`, `cmbTrayIconStyle`, the four alert spinboxes,
  `cmbDiskAnalyzer`, `btnManageSchedules`, `btnViewHistory`,
  `spnThresholdGB`). Relocate the shadow onto the 5 section cards at DS §2's
  alpha 90 / blur 26.
- Below-fold sections (Tools, Scheduled Cleaning, credit footer) are
  `.ui`-sourced, not screenshot-verified in this item's before/after — verify
  visually against `settings_page.ui:311-536` during implementation.
**Acceptance criteria (page-specific, in addition to the common list above):**
- Shadow count: **5** (one per `QGroupBox` section: General, Appearance,
  Alerts, Tools, Scheduled Cleaning) — verify the two 14-widget
  `addDropShadow` call sites are removed from both `init()` and
  `refreshThemeColors()`, not merely superseded visually
- No `@destructiveColor`/danger-role control on this page (per the note —
  `Manage Schedules…` / `View Cleaning History` are `accessibleName="primary"`
  → `@accentColor`; danger controls live only inside their dialogs, out of
  scope here)
- Section order, every control type, and all labels/values unchanged
  (including the below-fold Tools / Scheduled Cleaning sections and the
  Downloads sub-controls' indented sub-row layout)
- Sidebar highlights "Settings" (harness capture had a stale "Dashboard"
  highlight — CAPTURE_NOTES.md gotcha #5)
**UAT (business-user steps):**
1. Navigate to Settings. Verify General, Appearance, and Alerts each appear
   as separate cards with a colored bar and section title.
2. Scroll down and verify Tools and Scheduled Cleaning also appear as
   separate cards, matching the style of the sections above.
3. Change the Color Scheme dropdown to Light, confirm the whole app switches
   theme and all Settings cards remain legible and correctly styled.
4. Confirm every combo box, spinbox, and toggle in Settings still shows its
   previous value and still functions (nothing reset).
5. Confirm "Manage Schedules…" and "View Cleaning History" buttons still
   open their dialogs correctly.

---

### Boot Analysis (Paperclip: SSO-13744)
**Phase:** 2-Page
**Blocked by:** F1, F2, F3, F4
**Files:**
- `shared/nexis/Pages/BootAnalysis/boot_analysis_page.cpp`
- `shared/nexis/Pages/BootAnalysis/boot_analysis_page.h`
**Design refs:** DS §2 (elevated cards); DS §1 (surface hierarchy); DS §3
(header anatomy); DS §4 (hero-value typography); DS §5 (empty-state upgrade
of `#lblBootEmptyState`); notes/boot_analysis.md (approved 2026-07-15);
renders/boot_analysis_{dark,light}.png.
**Change summary:**
- Promote the bare "System uptime since last boot: 721091.0 s" text into a
  DS §2 elevated card with header anatomy — accent bar + "System uptime
  since last boot" title (`@color06`) + the value as the 14pt/700 hero
  (`@color05`).
- Apply the surface hierarchy (`@pageContent` / `@cardBgElevated`) so the
  uptime card and the notice card read as raised surfaces, not floating
  text.
- Rebuild the page toolbar: 3px accent bar + "Boot Analysis" title +
  "Startup timing" source line, keeping the existing top-right "Refresh"
  button.
- Upgrade the existing text-only empty notice (`#lblBootEmptyState`,
  `boot_analysis_page.cpp:78-83,145-146`) into the full empty-state pattern:
  clock icon + explanation + "Refresh uptime" next-action affordance, inside
  its own elevated card. Reword the copy per the approved note ("is not" →
  "isn't"; add "macOS does not expose per-service startup durations.").
**Acceptance criteria (page-specific, in addition to the common list above):**
- Shadow count: **2** (uptime card + empty-state notice card)
- Uptime value (721091.0 s) and the top-right Refresh button unchanged; no
  new controls or data invented
- `#lblBootEmptyState`'s upgrade to icon+explanation+action is the actual
  code change verified (not just a visual mockup match) — confirm the
  reworded copy matches the approved note exactly
**UAT (business-user steps):**
1. Navigate to Boot Analysis. Verify the uptime value now appears inside a
   bordered, shadowed card with a title above it, not as bare floating text.
2. Verify the empty-state notice (about per-service timing not being
   available on macOS) now shows a clock icon, explanation text, and a
   "Refresh uptime" button — not just a plain sentence.
3. Click "Refresh uptime" and confirm it attempts a refresh.
4. Click the existing top-right "Refresh" button and confirm it still works
   as before.
5. Switch to Light theme and confirm both cards remain legible.

---

## Deferred pages (no work item this round)

- **APT Source Manager** — Deferred by maintainer 2026-07-15 — no live
  capture; revisit once the skeleton can be verified against screenshots
  from an Ubuntu machine with the page available.
- **Docker** — Deferred by maintainer 2026-07-15 — no live capture; revisit
  once the skeleton can be verified against screenshots from an Ubuntu
  machine with the page available.
- **GNOME Settings** — Deferred by maintainer 2026-07-15 — no live capture;
  revisit once the skeleton can be verified against screenshots from an
  Ubuntu machine with the page available.

---

## Cross-check

- Count of `### ` headings in this file: **19** (4 Phase-1 + 15 Phase-2).
- Every note with `**Status:** Approved` has a corresponding Phase-2 item
  above: boot_analysis, disk_tools, hardware_info, helpers, homebrew,
  network_usage, processes, resources, search, services, settings,
  startup_apps, system_cleaner, system_logs, uninstaller (15).
- Every note with `**Status:** Deferred` is listed under "Deferred pages"
  with no work item: apt_source_manager, docker, gnome_settings (3).
- 15 + 3 = 18 total prototype pages, matching the spec's 18-page scope
  (Dashboard is the reference, not a prototype page).
