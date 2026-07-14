# Nexis Design System — v2.8.x Dashboard Language

**Status:** Normative. **Scope:** extracted from the Dashboard tile chrome shipped
in 2.8.x; binds the whole-app visual-consistency pass (see
`docs/superpowers/specs/2026-07-13-ux-modernization-design.md`).

**Audience:** (a) prototype builders producing token-true HTML mockups under
`docs/design/ux-modernization/mockups/`, and (b) Claude implementation agents
working a Paperclip (`NEX-*`) work item. This document is a recipe/rule
reference, not a history of how the language was designed — see the spec above
for rationale and the evidence pack under
`docs/design/ux-modernization/current-state/` for visual examples.

**Section anchors are stable.** `DS §1`–`DS §9` are cited by rationale notes and
work items; do not renumber or re-title a section.

**Citation conventions:**
- `file:line` — a constant verified directly in this repo as of 2026-07-14.
- `[KEY]` — an externally verified claim; the key resolves in
  `docs/design/ux-modernization/sources.md`, which also records any caveat on
  how far that source's wording actually goes. Where a claim in this document
  extrapolates beyond a source's literal text, that is called out inline rather
  than left implicit.
- Screenshots under `current-state/` illustrate sections but are never the
  source of a constant — every number and token name below traces to code.

---

## DS §1 — Surface hierarchy

Three layered surfaces, each theme token resolved by both themes:

| Layer | Token | Default (dark) | Light |
|---|---|---|---|
| Page background | `@pageContent` | `#1A1C22` | `#F5F0EB` |
| Card (flat) | `@cardBg` | `#2A2C32` | `#ffffff` |
| Card (elevated) | `@cardBgElevated` | `#32343A` | `#FFF8F2` |

Elevated surfaces (the top layer) are the only ones that carry a drop shadow —
flat cards sit directly on the page without one. Token declarations:
`values.ini:2,29,30` in both
`shared/nexis/static/themes/default/style/values.ini` and
`shared/nexis/static/themes/light/style/values.ini` (line numbers identical
across themes — confirmed). The elevated-surface-gets-shadow rule is the
Dashboard tile wrapper's behavior: `dashboard_tile_wrapper.cpp:107-130`.

## DS §2 — Card chrome

The elevated-card recipe (the unit every restyled page composes with):

- **Radius:** 12px
- **Border:** 1px solid `@borderColor`
- **Fill:** `@cardBgElevated` for the elevated wrapper; `@cardBg` for a flat
  card that doesn't need elevation (same radius/border, no shadow — see DS §1)
- **Shadow:** alpha 90, blur 26, offset `(0, 2)`, color `@shadowColor`
- **Outer margin:** 8px (clearance for the shadow so it doesn't clip against
  neighboring tiles)

Citations:
- `dashboard_tile_wrapper.cpp:107-130` (`applyDepthTreatment()`) — inline fill
  `@cardBgElevated` + border `@borderColor`, radius 12px, `setContentsMargins(8,
  8, 8, 8)`, and the `Utilities::addDropShadow(mInnerWidget, 90, 26)` call
  (alpha 90, blur 26).
- `utilities.h:21-32` (`Utilities::addDropShadow`) — resolves the shadow color
  from `@shadowColor` (line 24), then `setBlurRadius(blur)`, `setColor(...)`,
  `setOffset(0, 2)` (lines 30-32). The `(0, 2)` offset is hardcoded here, not a
  per-call parameter — every drop shadow in the app uses it.
- `style.qss:765-769` — the shared radius/border recipe (`border-radius: 12;
  border: 1px solid @borderColor;`) reused by every card-shaped selector
  (`#metricTile`, `#diskTile`, etc.), confirming radius/border are a
  repo-wide constant, not Dashboard-specific.

## DS §3 — Header anatomy

Every tile header: a 3px-wide, ≥26px-tall accent bar (radius 1) to the left of
a title row (title label + optional input-name label + stretch + a top-right
24×24 action button), with a muted source line beneath the title row.

- Root layout margins: `(14, 12, 14, 10)`; root spacing `6`; header-row
  spacing `8`.
- Action button (gear): fixed size 24×24, icon 14×14, `setAutoRaise(true)`,
  aligned `Qt::AlignTop | Qt::AlignRight` in the title row.
- Footer band: fixed height 24px.

Citations: `metric_tile_base.cpp:255-307` (`buildChrome()` — accent bar
`setFixedWidth(3)` / `setMinimumHeight(26)`, margins/spacing, title row,
source row) and `metric_tile_base.cpp:171-180` (`createGearButton()` — 24×24,
14×14 icon, autoRaise). Footer height: `metric_tile_base.h:65` (`FOOTER_HEIGHT = 24`),
applied at `metric_tile_base.cpp:335` with consistency shipped in 2.8.1
(`CHANGELOG.md:31,44`). Accent-bar radius 1 and the muted source-line color
(`@tertiaryText`) are QSS constants — see DS §4.

## DS §4 — Typography scale

| Role | Size / weight | Token | QSS selector |
|---|---|---|---|
| Title | 9pt / 600 | `@color06` | `#metricTileTitle` |
| Meta | 8pt / 500 | `@tertiaryText` | `#metricTileInput` |
| Hero value | 14pt / 700 | `@color05` | `#metricTileValue` |
| Pill | 8pt | (see DS §5) | `#metricTileTrend` |

Citation: `style.qss:772-815`. `@tertiaryText` and `@color05`/`@color06`
resolve identically in both themes (`values.ini:36` for `@tertiaryText`).

## DS §5 — Status & state patterns

- **Pill:** radius 8, padding `1 6`, background `@chartGridColor` — the same
  element cited for typography in DS §4. Citation: `style.qss:809-815`
  (`#metricTileTrend`).
- **Status color selectors:** generic `[status="…"]` property selectors drive
  runtime color choice without per-instance `setStyleSheet()` calls —
  `success`/`warning`/`error`/`info`/`dimmed`/`neutral`, plus a
  `#wizardStepIcon[status="…"]` size/weight variant. Citation:
  `style.qss:2421-2435`.
- **Empty state:** icon + a brief explanation + a next-action affordance
  [NNG-EMPTY]. This is a forward rule, not yet a repo-wide pattern: the one
  existing empty-state label (`boot_analysis_page.cpp:78-83,145-146`,
  `#lblBootEmptyState`) is text-only (no icon, no action) — restyled and new
  pages should upgrade to the full icon+explanation+action pattern, not copy
  that instance.
- **Loading:** a static skeleton placeholder, no animation. This follows
  directly from the no-timers/no-per-frame-animation guardrail (DS §9,
  item 2) rather than an external source — there is no existing skeleton
  implementation in the repo to cite as precedent; this is a clean-slate rule
  for any new loading state.

## DS §6 — Tracks, bars, charts

- **Track/grid color:** `@chartGridColor`. This was a deliberate 2.8.1 change
  — gauge/ring/progress/VU-meter/donut tracks moved off `@color02` because
  that value had become visually identical to the new elevated-card surface.
  Citation: `CHANGELOG.md:31,45` (`## [2.8.1]` entry).
- **Progress bar:** background `@chartGridColor`, radius 2, `max-height: 4`.
  Citation: `style.qss:817-826` (`#metricTileProgress`).
- **Series palette:** `@chartSeries01`–`@chartSeries20`, 20 fixed hex values
  per theme. Citation: `values.ini:60-79` (both themes; identical line range).

## DS §7 — Tables & lists

**Rule:** shadows live on the container, never on individual rows. A
table/list page gets one elevated container card (DS §2); rows inside it are
flat.

- **Current-state precedent that already matches this rule:** `QTableView`
  rows are flat — `background-color: @color01`, no per-item shadow, only a
  shared `gridline-color: @borderColor`. Citation: `style.qss:328-350`.
- **Current-state pattern this rule supersedes:** `ServiceItem` and
  `AptSourceRepositoryItem` each currently apply their own drop shadow per row
  (`Utilities::addDropShadow(this, 30, 10)`) — citations: `service_item.cpp:30`,
  `apt_source_repository_item.cpp:29`. Restyled list-of-rows pages (Services,
  StartupApps, APT) move to a single elevated container holding flat
  `@cardBg` rows instead, per the modernization spec's own risk mitigation:
  `docs/superpowers/specs/2026-07-13-ux-modernization-design.md:121`
  ("flat `@cardBg` rows inside one elevated container for lists/tables").
- **Density & alignment:** borders, zebra striping, hover-highlight, and
  frozen header rows/columns aid scanning and comparison [NNG-TABLES]. Caveat
  (see `sources.md`): the NN/g article explicitly recommends borders,
  zebra striping, hover-highlight, and frozen headers for scanning — it does
  not use the word "density" and does not discuss numeric-column alignment;
  treat those two specifics as reasonable extrapolation from the article's
  broader scanning guidance, not a verbatim NN/g recommendation.
- **Platform notes:** macOS layout/margin conventions for table/list
  containers [HIG-MACOS]; GNOME boxed-list row/spacing conventions relevant to
  Linux list-of-rows pages (Services, APT) [GNOME-HIG].

## DS §8 — Theming rules

- **Token set:** 79 tokens × 2 themes (`@key=#hex`, flat `values.ini` per
  theme), substituted into one master QSS template by
  `AppManager::updateStylesheet()`. Citation: 79 `@`-prefixed keys counted in
  both `shared/nexis/static/themes/default/style/values.ini` and
  `shared/nexis/static/themes/light/style/values.ini`.
- **Parity is test-enforced,** not just convention:
  `tests/theme/test_theme_tokens.cpp` — `themes_sameTokenSets()` (token-set
  parity between themes), `darkTheme_allTokensResolved()` /
  `lightTheme_allTokensResolved()` (every QSS `@token` exists in both
  `values.ini` files), `noRawTokensInPerWidgetStyleSheet()` (per-widget
  `setStyleSheet()` calls must not contain an unresolved `@token` literal —
  those never go through the global substitution pass), and the mirrored
  `darkTheme_allCppTokensResolved()` / `lightTheme_allCppTokensResolved()` for
  `sv->value("@token")` reads in C++.
- **BUG-47** (no hardcoded colors in C++): all widgets read colors from
  `values.ini` via `AppManager::getStyleValues()`; zero literal hex in C++.
  Citation: `CHANGELOG.md:692`.
- **BUG-49** (token names must not be substrings of other tokens): token
  replacement is sorted by descending key length before substitution, so
  e.g. `@sidebar` cannot clobber `@sidebarDivider`. Citation:
  `CHANGELOG.md:694`; mechanism at `shared/nexis/Managers/app_manager.cpp:188-194`.
- **`refreshThemeColors()` is only for painted colors** — i.e., colors a
  widget draws itself in a custom `paintEvent` (arcs, tracks, sparklines),
  which QSS cannot reach and which therefore need an explicit re-resolve +
  `update()` on theme change. `refreshThemeColors()` is declared pure virtual
  on the tile base (`metric_tile_base.h:35`) and implemented per tile; example:
  `gauge_tile.cpp:84-101` resolves `@chartGridColor`/`@color05`/`@tertiaryText`
  into painted members (`mTrackColor`, `mTextColor`, `mSecondaryTextColor`)
  and calls `update()`. QSS-styled (non-painted) colors don't need this — the
  global stylesheet reapplies them automatically on theme change.
- **Inline styles resolve tokens at runtime, not at write time:** a
  per-widget `setStyleSheet()` call must interpolate an already-resolved value
  (`sv->value("@token", fallback).toString()`) rather than embed a raw
  `@token` literal, because only `qApp->setStyleSheet()` goes through
  `AppManager`'s substitution pass. Enforced by
  `noRawTokensInPerWidgetStyleSheet()` in `tests/theme/test_theme_tokens.cpp`.

## DS §9 — Performance guardrails

The six guardrails binding every work item (spec §5,
`docs/superpowers/specs/2026-07-13-ux-modernization-design.md:92-99`):

1. **QSS-first** — restyle through the single global stylesheet wherever
   possible, rather than per-widget inline styles [QT-QSS].
2. **No new timers, polling, or per-frame animation.** Static chrome only.
3. **Bounded shadows** — cap shadow-bearing containers per page at
   dashboard-proven counts; container-level cards, never per-row shadows (DS
   §7). [QT-SHADOW] documents only the effect's existence and mechanism
   (a configurable blur/offset/color drop shadow rendered via
   `QGraphicsDropShadowEffect`) — it does not document repaint cost, so that
   part of the claim is not sourced to the Qt page. The bounding rationale
   instead rests on in-repo precedent: the Dashboard applies exactly one
   `addDropShadow()` call per tile wrapper (`dashboard_tile_wrapper.cpp:130`),
   while Settings has two call sites (`settings_page.cpp:213` at init,
   `settings_page.cpp:768` in `refreshThemeColors()`) that each apply
   `addDropShadow()` to the *same* fixed list of 14 widgets — a widget's
   effect is replaced via `setGraphicsEffect()`, not stacked, so re-init or a
   theme switch never grows the shadow count unboundedly.
4. **Lazy construction preserved (FR-97)** — non-Dashboard pages build on
   first navigation via the `PageSlot` factory registry, not at launch.
   Citations: `shared/nexis/app.cpp:660-663`; `CHANGELOG.md:365-366`.
5. **Re-polish discipline (BUG-56)** — after changing a QSS dynamic property,
   call `unpolish()`/`polish()` on the affected widget so the property
   selector re-evaluates; Qt does not do this automatically
   [QT-QSS-SYNTAX]. Concrete instance: `metric_tile_base.cpp:37`.
6. **Measured acceptance** — cold-start and idle CPU must stay unchanged
   within noise on both platforms per work item. Spec citation:
   `docs/superpowers/specs/2026-07-13-ux-modernization-design.md:98-99`. The
   FR-96/FR-97/FR-98 cold-launch bundle (`CHANGELOG.md:365-368`) is the
   existing measurement discipline to match, not a guardrail this document
   invents.
