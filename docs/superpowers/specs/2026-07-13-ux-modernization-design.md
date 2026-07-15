# UX Modernization — Whole-Application Visual Consistency Pass

**Date:** 2026-07-13 (rescoped 2026-07-14 after maintainer review)
**Status:** Approved intent; prototype-approval gate pending
**Owner:** Luke Simpson (EngineeringLead) · Implementation by Claude agents via Paperclip (NEX items)

## 1. Goal

Restyle every page of Nexis to match the v2.8.x dashboard design language
(GH#191, GH#213, GH#214) — its **shadow depths**, **surface texturing** (warm
elevated-card tones), and **layout consistency**. This phase produces **image
prototypes for maintainer approval**, not code: Luke views a before/after image
pair for every page and approves or requests revisions **before any page is
changed**. Only approved prototypes become Paperclip work items.

### Scope decisions (locked)

| Decision | Choice |
|---|---|
| Depth | **Visual consistency pass** — dashboard language (elevated cards, header/section anatomy, theme tokens, spacing/typography) applied to all pages. Layouts and interactions stay structurally unchanged. |
| Approval gate | **Image prototypes reviewed page-by-page by Luke before implementation.** Delivered as before/after PNG pairs (dark + light) in-repo, plus a click-through review gallery (private Claude artifact). |
| Current-state capture | **Zero code changes.** The existing screenshot test binary already writes capture PNGs to `build/test_screenshots/` on every run (12 pages); remaining pages are captured by running the app and screenshotting it. The test harness, `kPageMap`, and committed baselines are untouched. |
| Prototype format | Styled HTML built from real `values.ini` token values, rendered to PNG. |
| Work decomposition | **Foundation first, then per-page**: shared components/QSS patterns encode the language once; page items apply them. |
| Sourcing policy | Recommendations grounded in the dashboard code (file:line) or, where the dashboard has no precedent, a cited authority (Apple HIG, GNOME HIG, Qt 6 docs, NN/g). Rationale stays brief — one note per page attached to its prototype. |

### Non-goals

- **No code changes in this phase — including tests.** The screenshot harness and its baselines are explicitly out of scope.
- No navigation/sidebar or information-architecture changes.
- No new interactions, pages, or features; no layout restructuring beyond card/section chrome.
- No performance regressions traded for appearance (§5).

## 2. Ground truth (research this design rests on)

- **Design language location:** tile anatomy in `MetricTileBase`
  (`shared/nexis/Pages/Dashboard/metric_tile_base.cpp`: accent bar 3px×≥26px r1,
  margins 14,12,14,10, footer 24px, gear 24×24) and elevation in
  `DashboardTileWrapper` (`dashboard_tile_wrapper.cpp:107-130`: `@cardBgElevated`,
  12px radius, 1px `@borderColor`, `Utilities::addDropShadow(90, 26)` = alpha 90,
  blur 26, offset (0,2), `utilities.h:30-32`). Typography scale in
  `style.qss:763-833` (title 9pt/600, meta 8pt/500, hero 14pt/700, pill 8pt r8 on
  `@chartGridColor`). The chrome classes are Dashboard-internal; the tokens and
  `addDropShadow()` are already shared — so the foundation promotes the *language*,
  not the tile widget.
- **Theming:** 79 tokens × 2 themes (`values.ini`, flat `@key=#hex`), one master
  QSS template (2,836 lines) substituted by `AppManager::updateStylesheet()`.
  Test-enforced: token-set parity, no raw tokens in per-widget stylesheets, BUG-47
  (no hardcoded colors), BUG-49 (non-substring names). 10 of 17 shared pages are
  global-QSS-only — most restyling is centralized QSS work.
- **Pages:** 17 shared + Homebrew (macOS) + APT Source Manager (Linux). Dashboard
  is the reference; **18 pages get prototypes**. Structure classes: table-centric
  (Processes, Search, SystemLogs, BootAnalysis, HardwareInfo, Uninstaller),
  list-of-rows (Services, StartupApps, APT), tree/stacked tools (SystemCleaner,
  DiskTools, Docker, Helpers, Homebrew), charts (Resources, Network), forms
  (Settings, GnomeSettings). Docker/GnomeSettings/Homebrew/APT are
  runtime/platform gated.
- **Capture paths that need no code change:** the test binary saves every page it
  visits to `build/test_screenshots/<platform>/<theme>/` even when comparisons
  fail; `scripts/update_screenshots.sh` (generate mode, run on a throwaway clone)
  produces the same set on Linux where no baselines exist. Pages outside the
  harness's 12 are captured by launching the app and screenshotting each page.

## 3. Deliverables

1. **Evidence pack** — `docs/design/ux-modernization/current-state/{macos,linux}/{dark,light}/*.png`
   — current-state captures of every reachable page, plus `CAPTURE_NOTES.md`
   logging any page that could not be captured live (e.g. GnomeSettings on a
   headless homelab) and what its prototype derives from instead.
2. **`docs/design/DESIGN_SYSTEM.md`** — the dashboard language as a short
   normative spec (surfaces, card recipe, header anatomy, typography, states,
   guardrails), every constant cited to file:line; external claims cited in
   `docs/design/ux-modernization/sources.md`.
3. **Prototypes** — per page: `mockups/pages/<page>.html` (token-true HTML) and
   `mockups/renders/<page>_{dark,light}.png`, each with a brief rationale note
   (`mockups/notes/<page>.md`: what changes, why, source).
4. **Review gallery** — a private artifact showing before/after side-by-side per
   page for Luke's approval; revised until each page is approved or explicitly
   deferred.
5. **Paperclip work-item package** — `work-items.md` drafts (foundation items +
   one item per **approved** page), each self-contained for an implementing agent.

## 4. Approval gate (the point of this phase)

No Phase-2 page work item is filed until Luke has approved that page's prototype
from the review gallery. Feedback loops per page: revise prototype → re-render →
re-review. Pages Luke defers are dropped from `work-items.md`, not carried as
assumptions.

## 5. Performance guardrails (binding on every work item)

1. **QSS-first** — restyle through the single global stylesheet wherever possible.
2. **No new timers, polling, or per-frame animation.** Static chrome only.
3. **Bounded effects** — cap shadow-bearing containers per page at
   dashboard-proven counts; container-level cards, never per-row shadows.
4. **Lazy construction preserved** (FR-97).
5. **Re-polish discipline** (BUG-56).
6. **Measured acceptance:** cold-start and idle CPU unchanged within noise on
   both platforms per work item.

## 6. Work-item packaging (Paperclip / NEX)

- **Phase 1 — Foundation (~4 items):** shared card container (DS card recipe),
  section-header pattern, state patterns (empty/loading/status pill), QSS
  consolidation groundwork. Encodes consistency in shared code, not prose.
- **Phase 2 — Pages (up to 18 items, parallel, blocked on Phase 1):** one item per
  approved page, containing: before-PNGs, approved prototype (PNG + HTML),
  rationale note, exact file list, acceptance criteria (visual match to approved
  prototype in both themes, guardrails §5, CHANGELOG + docs updates), and
  business-user UAT steps. Suggested order: Processes, Resources, SystemCleaner
  first.

## 7. Risks & mitigations

- **Linux-gated pages without live captures** (GnomeSettings on headless homelab)
  → prototype derives from `.ui` files, banner-flagged, verified visually during
  implementation UAT.
- **Agents drifting from approved prototypes** → acceptance = match to the
  approved render; shared Phase-1 components carry the recipes.
- **Shadow cost on long scroll pages** → guardrail §5.3; flat `@cardBg` rows
  inside one elevated container for lists/tables.

## 8. Acceptance criteria for this phase

- [x] Evidence pack covers every page reachable on macOS + homelab Linux (16 macOS + 12 Linux pages × 2 themes); gaps logged in CAPTURE_NOTES.md.
- [x] DESIGN_SYSTEM.md constants all traceable to file:line (citation-audited in review); external claims cited in sources.md.
- [x] 18 prototype pairs (dark+light, 38 renders incl. smoke) rendered from token-true HTML, each with a sourced rationale note (grep-verified: no note lacks Source lines).
- [x] Review gallery delivered 2026-07-14; verdicts 2026-07-15 — 15 pages approved (incl. all 7 maintainer-judgment items), 3 deferred (docker, apt_source_manager, gnome_settings).
- [x] work-items.md covers Phase 1 (F1–F4) + every approved page (19 items); deferred pages listed without items.
- [x] `git diff` vs merge-base 20669f0a shows **no changes** under `shared/`, `linux/`, `macos/`, or `tests/` — documentation and design tooling (`scripts/design/`, `docs/design/`) only.
