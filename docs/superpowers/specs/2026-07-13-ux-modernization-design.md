# UX Modernization — Whole-Application Visual Consistency Pass

**Date:** 2026-07-13
**Status:** Approved design, pending written-spec review
**Owner:** Luke Simpson (EngineeringLead) · Implementation by Claude agents via Paperclip (NEX items)

## 1. Goal

Extend the v2.8.x dashboard design language (GH#191, GH#213, GH#214) to every page of
Nexis so the whole application reads as one product. This effort produces a **plan and
design package only** — no application code changes. All implementation is executed later
by Claude agents working from Paperclip work items, so every design artifact must be
self-contained and machine-actionable.

### Scope decisions (locked)

| Decision | Choice |
|---|---|
| Depth | **Visual consistency pass** — extend the dashboard language (elevated cards, unified header/footer anatomy, theme tokens, spacing/typography). Layouts and interactions stay structurally unchanged. |
| Platform coverage | All pages on both platforms. macOS screenshots captured locally; Linux screenshots via the `screenshot-baselines.yml` CI workflow (x64) plus homelab captures for runtime-gated pages. |
| Design artifact format | **HTML mockups + rendered PNGs** built from real `values.ini` token values. |
| Work decomposition | **Foundation first, then per-page**: shared components/QSS patterns encode the language once; page items apply them. |
| Sourcing policy | **Approach B** — internal extraction (code refs + screenshots) for consistency claims; external citations (Apple HIG, GNOME HIG, Qt 6 docs, NN/g) only where the dashboard offers no precedent. Every recommendation carries a source. |

### Non-goals

- No navigation/sidebar or information-architecture changes.
- No new interactions, pages, or features; no layout restructuring beyond card/section chrome.
- No monetization-adjacent or platform-expansion work (MAINTAINER_SOP escalation domain).
- No performance regressions traded for appearance (see §6).

## 2. Ground truth (research findings this design rests on)

### 2.1 The design language is fully specified in code

The v2.8.x tile language lives in `shared/nexis/Pages/Dashboard/`:

- **`MetricTileBase`** (`metric_tile_base.{h,cpp}`) — unified anatomy: 3px × ≥26px accent
  bar (radius 1), two-line header (title row + source sub-header), 24×24 gear pinned
  top-right, fixed 24px footer band (`FOOTER_HEIGHT`, `metric_tile_base.h:65`) with hero
  value + trend pill. Root margins 14,12,14,10; spacing 6 (`metric_tile_base.cpp:257-258`).
- **`DashboardTileWrapper`** (`dashboard_tile_wrapper.cpp:107-130`) — elevation treatment:
  `@cardBgElevated` fill, 12px radius, 1px `@borderColor`, drop shadow via
  `Utilities::addDropShadow(widget, 90, 26)` (alpha 90, blur 26, offset (0,2),
  `utilities.h:30-32`), wrapper margins 8px for shadow room.
- **`TileValueFit::fittedPixelSize()`** (`tile_value_fit.cpp:8-31`) — fit-to-shape value
  scaling (GH#214).
- **Typography scale** (`style.qss:763-833`): title 9pt/600 `@color06`; source/input 8pt
  `@tertiaryText`; hero value 14pt/700 `@color05`; trend pill 8pt on `@chartGridColor`,
  padding 1 6, radius 8.

Reusability status: `MetricTileBase`/`DashboardTileWrapper` are referenced **nowhere
outside Dashboard**; `Utilities::addDropShadow()` and the tokens are already shared
(`@cardBgElevated` already used by 7 QSS blocks outside the dashboard). The foundation
work therefore promotes the *language*, not the tile widget.

### 2.2 The theming system enforces the constraints agents must obey

- 2 themes (`default` = dark, `light`), **79 tokens each**, defined in
  `shared/nexis/static/themes/{default,light}/style/values.ini`. A single master QSS
  template (`themes/default/style/style.qss`, 2,836 lines) has `@token` placeholders
  substituted at load by `AppManager::updateStylesheet()` (`app_manager.cpp:125-257`),
  longest-key-first (BUG-49 guard, lines 188-195).
- Test-enforced invariants (`tests/theme/test_theme_tokens.cpp`): identical token sets in
  both themes (`themes_sameTokenSets`), every C++-referenced token resolvable in both
  themes, no raw `@token` literals in per-widget stylesheets
  (`noRawTokensInPerWidgetStyleSheet`), all values valid hex.
- Theme-change pattern: `refreshThemeColors()` + `SignalMapper::sigChangedAppTheme`
  (25 implementers, 34 connect sites). Required only for painted/dynamic colors.
- **10 of 17 shared pages style themselves via the global QSS only** (BootAnalysis,
  Docker, GnomeSettings, Processes, Resources, Search, Services, StartupApps,
  SystemCleaner, Uninstaller) — for these, most of the restyle is QSS work, not C++.

### 2.3 Page inventory (audit targets)

17 shared pages plus 2 platform pages. Structure classes for audit and mockup purposes:

| Structure | Pages |
|---|---|
| Tile grid (done — reference) | Dashboard |
| Table-centric | Processes, Search, SystemLogs, BootAnalysis, HardwareInfo (spec tables), Uninstaller (table + tree) |
| List-of-rows | Services, StartupApps, APT Source Manager (Linux) |
| Tree/stacked tools | SystemCleaner, DiskTools, Docker (tabs + trees), Helpers (tool hub), Homebrew (macOS) |
| Charts | Resources, Network |
| Forms | Settings, GnomeSettings (Linux) |

Conditional registration: Docker (`checkDocker()`), GnomeSettings (Linux +
`checkGnomeSettings()`), Homebrew/APT (`checkSourceRepository()`).

### 2.4 Screenshot harness

`tests/screenshots/test_screenshots.cpp` captures a 1024×768 Fusion-style window,
12 pages × dark/light, compared with channel fuzz ≤8 and 1% tolerance, dynamic regions
masked. Baselines exist for **macOS only** (`tests/reference_screenshots/macos/{dark,light}/`,
12 PNGs each; stale for the dashboard — predate 2.8.x, GH#224). **Not captured:**
BootAnalysis, DiskTools, Docker, GnomeSettings, SystemLogs, and the platform pages.
Regeneration: `NEXIS_GENERATE_REFS=1` via `scripts/update_screenshots.sh` locally, or the
`workflow_dispatch` workflow `screenshot-baselines.yml` (Linux x64 in `ubuntu:26.04`
container + macos-14; Linux ARM64 skipped — hangs under xvfb).

## 3. Deliverables

All in-repo, produced by the planning effort (this project), consumed by Paperclip agents:

1. **`docs/design/DESIGN_SYSTEM.md`** — normative design-system spec extracted from the
   dashboard code: surface hierarchy (`@pageContent` → `@cardBg` → `@cardBgElevated`),
   card chrome recipe, header/section anatomy, typography scale, state patterns, spacing
   rules, and the performance guardrails (§6). Every constant cites file:line in v2.8.x
   code; every judgment-call pattern cites an external authority.
2. **Evidence pack** — `docs/design/ux-modernization/current-state/{macos,linux}/{dark,light}/`:
   fresh full-page screenshots of every page (including the 5 currently uncaptured pages
   and both platform pages).
3. **Per-page audit sheets** — `docs/design/ux-modernization/audits/<page>.md`: current
   screenshots, deviation list against DESIGN_SYSTEM.md, recommendations. Each finding
   carries a `Source:` line — code ref, screenshot ref, or citation.
4. **Target mockups** — `docs/design/ux-modernization/mockups/<page>.html` + rendered
   `.png` (dark and light), built with the actual 79-token palette so measurements and
   colors are copy-exact for agents.
5. **Paperclip work-item package** — `docs/design/ux-modernization/work-items.md`: drafts
   for every NEX item (§7), ready to paste into Paperclip.

## 4. Evidence pipeline

1. **Harness extension (test-only change, approved):** add BootAnalysis, DiskTools,
   Docker, GnomeSettings, SystemLogs (+ Homebrew/APT where present) to `kPageMap` with
   appropriate dynamic-region masks. No app code is touched.
2. **macOS captures:** local build; `scripts/update_screenshots.sh` in generate mode for
   dark + light.
3. **Linux captures:** dispatch `screenshot-baselines.yml` (linux-x64), download the
   artifact. Pages gated on runtime tools that the CI container lacks (Docker, GNOME,
   APT) are captured on the Ubuntu homelab with the same script (offscreen platform is
   supported, SSO-3729).
4. Captures land in the evidence pack (deliverable 2); dashboard baselines regenerated in
   passing resolve the GH#224 staleness waiver.

## 5. Foundation components (Phase-1 scope)

Promote the language into shared, reusable form. Final list is confirmed by the audits,
but the research fixes the shape:

- **Card container** — a shared widget or QSS class applying the elevated-card recipe
  (`@cardBgElevated`, 12px radius, 1px `@borderColor`, `addDropShadow(90, 26)`) for use as
  a section container on content pages. Dashboard tiles keep their own wrapper.
- **Section header pattern** — accent bar + title + muted sub-line, adapted from the tile
  header anatomy (3px bar, 9pt/600 title, 8pt source line).
- **State patterns** — empty state, loading state, and status pill styles, generalizing
  the trend pill and existing `[status="…"]` selectors.
- **Token additions** (only if audits demand them) — must satisfy: non-substring names
  (BUG-49), defined in both `values.ini` files (test-enforced parity), no hardcoded
  colors in C++ (BUG-47), valid hex.
- **QSS consolidation** — page-specific styling moves into the master template under
  object-name/property selectors; per-widget `setStyleSheet()` only where tokens must be
  resolved at runtime (painted widgets), per the existing convention.

## 6. Performance guardrails (binding on every work item)

1. **QSS-first.** Restyle through the single global stylesheet wherever possible.
   Per-widget `setStyleSheet()` bypasses token substitution and triggers extra re-polish
   (`noRawTokensInPerWidgetStyleSheet` polices the token half; the audit flags the rest).
2. **No new timers, polling, or per-frame animation.** Static chrome only.
3. **Bounded effects.** `QGraphicsDropShadowEffect` repaints cost real CPU; cap
   shadow-bearing containers per page at dashboard-proven counts (a full tile grid is the
   ceiling). Citation to Qt rendering docs goes in DESIGN_SYSTEM.md.
4. **Lazy construction preserved** (FR-97). No restyle may force eager page
   instantiation or add work to `ensureAllPages()` outside tests.
5. **Re-polish discipline** (BUG-56). Dynamic-property changes must explicitly
   unpolish/polish affected children; avoid designs that toggle properties frequently.
6. **Measured acceptance:** cold-start wall time and idle-state CPU unchanged within
   noise on both platforms, checked in each page item's verification step.

## 7. Work-item packaging (Paperclip / NEX)

**Phase 1 — Foundation (~3–5 items, ordered):**
harness coverage extension (already needed for evidence; filed for the record), card
container, section header pattern, state patterns, QSS consolidation groundwork.

**Phase 2 — Pages (~19 items, parallel, each blocked only by Phase 1):**
one item per page: Processes, Resources, SystemCleaner, Services, Uninstaller,
StartupApps, Network, HardwareInfo, DiskTools, Search, Settings, Helpers, SystemLogs,
BootAnalysis, Docker, GnomeSettings, plus Homebrew (macOS) and APT Source Manager
(Linux). Suggested order: high-traffic first (Processes, Resources, SystemCleaner).

**Every page item is self-contained and includes:**
- Before-PNG paths (evidence pack) and target mockup paths (HTML + PNG).
- DESIGN_SYSTEM.md section references for each change.
- Exact file list (page .ui/.cpp, QSS block line ranges).
- Acceptance criteria: visual match to mockup within screenshot tolerance, both themes,
  guardrails §6, screenshot baselines regenerated, `CHANGELOG.md` +
  `docs/APPLICATION_OVERVIEW.md` updated per project rules.
- UAT steps in business-user language (global Phase-4 workflow).

## 8. Risks & mitigations

- **Linux CI container can't show gated pages** → homelab capture step (§4.3).
- **Screenshot flakiness under xvfb on Linux ARM64** → x64-only CI captures; ARM64 visual
  verification is manual during UAT.
- **Agents drifting from the design language** → consistency enforced by shared Phase-1
  code, not prose; mockups carry exact measurements; acceptance = screenshot comparison.
- **Shadow effects regressing scroll performance on long pages** → guardrail §6.3 cap +
  per-item idle-CPU check; fall back to borderless flat cards (`@cardBg`) where a page
  hosts many small rows (lists/tables get container-level cards, not per-row shadows).

## 9. Acceptance criteria for this planning effort

- [ ] DESIGN_SYSTEM.md exists; every constant traceable to file:line; every external
      claim cited with a URL.
- [ ] Evidence pack contains all pages × both themes × both platforms.
- [ ] One audit sheet per page; zero unsourced recommendations.
- [ ] One mockup (HTML + dark/light PNGs) per page.
- [ ] work-items.md drafts cover Phase 1 + all Phase-2 pages with dependencies noted.
- [ ] No application source files modified (only `tests/screenshots/` +
      new docs/design artifacts).
