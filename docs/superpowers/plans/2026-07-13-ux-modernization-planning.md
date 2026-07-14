# UX Modernization Planning Package — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Produce the complete UX-modernization planning package defined in
`docs/superpowers/specs/2026-07-13-ux-modernization-design.md` — evidence screenshots of
every page, a normative design-system spec, sourced per-page audits, HTML+PNG target
mockups, and ready-to-file Paperclip work-item drafts.

**Architecture:** This is a documentation/evidence effort, not an app change. The only
compiled-code change is extending the screenshot test harness (`kPageMap`) so every page
can be captured. Everything else lands under `docs/design/` and `scripts/design/`.
Consistency is achieved by extracting the v2.8.x dashboard language into
`DESIGN_SYSTEM.md` once, then auditing and mocking every page against it.

**Tech Stack:** Qt Test screenshot harness (C++), `gh` CLI (workflow dispatch/artifact
download), Python 3 (token→CSS generation), Playwright via `npx` (mockup rendering),
Markdown deliverables.

## Global Constraints

- Branch: `claude/ux-modernization-spec` (repo default branch is `native`; never commit or push to it).
- The ONLY app-repo code file that may be modified: `tests/screenshots/test_screenshots.cpp`. New files are allowed only under `docs/design/`, `docs/superpowers/`, and `scripts/design/`.
- No application source, `.ui`, QSS, or `values.ini` changes anywhere in this plan.
- Every audit recommendation must carry a `Source:` line — a repo `file:line`, an evidence screenshot path, or a URL from `sources.md`. Zero unsourced recommendations (spec §9).
- Mockups may use only colors from `shared/nexis/static/themes/{default,light}/style/values.ini` (79 tokens per theme), via the generated `tokens.css`.
- Any *proposed* new tokens in audit/work-item docs must obey: non-substring names (BUG-49), defined in both themes (test `themes_sameTokenSets`), no hardcoded colors in C++ (BUG-47).
- Performance guardrails (spec §6) are binding content for DESIGN_SYSTEM.md and every work-item draft: QSS-first, no new timers/animation, bounded shadow effects, lazy construction preserved (FR-97), re-polish discipline (BUG-56), measured cold-start/idle-CPU acceptance.
- Conventional commits; reference `GH#224` where baselines are touched. Do not merge the PR — Luke reviews.
- Directory layout for deliverables:
  - `docs/design/DESIGN_SYSTEM.md`
  - `docs/design/ux-modernization/current-state/{macos,linux}/{dark,light}/*.png`
  - `docs/design/ux-modernization/sources.md`
  - `docs/design/ux-modernization/audits/<snake_case_page>.md`
  - `docs/design/ux-modernization/mockups/{tokens.css,base.css,pages/*.html,renders/*.png}`
  - `docs/design/ux-modernization/work-items.md`

---

### Task 1: Extend the screenshot harness to cover every page

**Files:**
- Modify: `tests/screenshots/test_screenshots.cpp:41-74` (PageInfo struct + kPageMap) and `:260-288` (capture loop)

**Interfaces:**
- Produces: screenshot names consumed by Tasks 2–4 and all audit tasks:
  `boot_analysis`, `disk_tools`, `system_logs`, `docker`, `gnome_settings`,
  `homebrew`, `apt_source_manager` (plus the existing 12).

- [ ] **Step 1: Add the `optional` field to `PageInfo`**

In `tests/screenshots/test_screenshots.cpp`, change the struct (lines 41–49) to:

```cpp
struct PageInfo {
    QString className;
    QString screenshotName;
    // Child widget classes (matched via QObject::inherits) whose on-screen
    // rectangles are masked out before comparison.
    QStringList dynamicClassNames;
    // Child widget objectNames whose rectangles are masked out.
    QStringList dynamicObjectNames;
    // Conditionally-registered pages (ToolManager or platform gating) are
    // skipped when absent from the stacked widget instead of failing the run.
    bool optional = false;
};
```

- [ ] **Step 2: Append the seven missing pages to `kPageMap`**

After the `{"SettingsPage", "settings", {}, {}},` entry (line 73), add:

```cpp
    {"BootAnalysisPage",     "boot_analysis",      {"QAbstractItemView"}, {}},
    {"DiskToolsPage",        "disk_tools",         {"QAbstractItemView"}, {}},
    {"SystemLogsPage",       "system_logs",        {"QAbstractItemView"}, {}},
    // Conditionally registered (ToolManager / platform gating) — optional:
    {"DockerPage",           "docker",             {"QAbstractItemView"}, {}, true},
    {"GnomeSettingsPage",    "gnome_settings",     {"QAbstractItemView"}, {}, true},
    {"HomebrewPage",         "homebrew",           {"QAbstractItemView"}, {}, true},
    {"APTSourceManagerPage", "apt_source_manager", {"QAbstractItemView"}, {}, true},
```

(All seven mask `QAbstractItemView` because their tables/trees/lists render
machine-dependent data — same convention as `ProcessesPage`.)

- [ ] **Step 3: Skip absent optional pages in the capture loop**

Replace lines 260–264:

```cpp
        for (const auto &page : kPageMap) {
            QWidget *widget = findPageByClassName(page.className);
            QVERIFY2(widget, qPrintable(QString("Page widget '%1' not found in stacked widget "
                                                "— check kPageMap and App::ensureAllPages()")
                                        .arg(page.className)));
```

with:

```cpp
        for (const auto &page : kPageMap) {
            QWidget *widget = findPageByClassName(page.className);
            if (!widget && page.optional) {
                qInfo() << "Skipping optional page (not registered in this"
                        << "build/environment):" << page.className;
                continue;
            }
            QVERIFY2(widget, qPrintable(QString("Page widget '%1' not found in stacked widget "
                                                "— check kPageMap and App::ensureAllPages()")
                                        .arg(page.className)));
```

- [ ] **Step 4: Skip optional pages with no committed baseline in compare mode**

After the existing `const QString refPath = themeRefDir + ...;` line in the
compare branch (line 284), before the `QVERIFY2(QFile::exists(refPath), ...)`:

```cpp
            if (!QFile::exists(refPath) && page.optional) {
                qInfo() << "Skipping optional page (no committed baseline):"
                        << page.screenshotName;
                continue;
            }
```

(Optional pages appear only on machines whose environment registers them —
e.g. Homebrew on macOS CI, APT in the Linux container. Without this, the first
machine that *has* the page but no baseline hard-fails.)

- [ ] **Step 5: Build the test target**

Run: `cmake --build build --target test-ScreenshotTests -j$(sysctl -n hw.ncpu)`
(If `build/` doesn't exist: `cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=$(brew --prefix qt@6)` first.)
Expected: compiles with no errors.

- [ ] **Step 6: Verify compare mode still passes against existing baselines**

Run: `ctest --test-dir build -R ScreenshotTests --output-on-failure`
Expected: PASS. The 12 existing pages compare clean; the 3 new always-present
pages hard-fail on "Reference missing"? **No** — they are non-optional and have
no baselines yet, so this run is expected to FAIL with `Reference missing:
.../boot_analysis.png`. That failure is the correct signal that Step 7 (generate
mode, Task 2) must run before this branch's CI is green. If instead the failure
is `Page widget 'X' not found`, the class name in Step 2 is wrong — fix before
proceeding.

- [ ] **Step 7: Commit**

```bash
git add tests/screenshots/test_screenshots.cpp
git commit -m "test(screenshots): cover all pages in kPageMap, skip absent optional pages"
```

---

### Task 2: Capture macOS evidence + regenerate macOS baselines (GH#224)

**Files:**
- Modify: `tests/reference_screenshots/macos/{dark,light}/*.png` (regenerated + new)
- Create: `docs/design/ux-modernization/current-state/macos/{dark,light}/*.png`

**Interfaces:**
- Consumes: Task 1's extended `kPageMap`.
- Produces: the macOS half of the evidence pack used by every audit task.

- [ ] **Step 1: Regenerate macOS baselines (generate mode)**

Run: `./scripts/update_screenshots.sh`
Expected: `Generated: .../macos/dark/<name>.png` and `.../light/<name>.png` lines —
15 always-present pages per theme, plus `homebrew` (this Mac has brew) and
`docker` if Docker Desktop is registered. `gnome_settings` and
`apt_source_manager` must be skipped with the "optional page" message.

- [ ] **Step 2: Sanity-check the output set**

Run: `ls tests/reference_screenshots/macos/dark/ | sort`
Expected: the 12 previous names plus at least `boot_analysis.png`,
`disk_tools.png`, `system_logs.png`, `homebrew.png`. Dark and light dirs contain
identical file lists (`diff <(ls .../dark) <(ls .../light)` → empty).

- [ ] **Step 3: Verify compare mode is now green**

Run: `ctest --test-dir build -R ScreenshotTests --output-on-failure`
Expected: PASS (this also clears the RELEASE.md §0.5 dashboard-baseline waiver, GH#224).

- [ ] **Step 4: Copy captures into the evidence pack**

```bash
mkdir -p docs/design/ux-modernization/current-state/macos
cp -R tests/reference_screenshots/macos/dark  docs/design/ux-modernization/current-state/macos/dark
cp -R tests/reference_screenshots/macos/light docs/design/ux-modernization/current-state/macos/light
```

- [ ] **Step 5: Visually spot-check three PNGs**

Open `current-state/macos/dark/dashboard.png`, `processes.png`, `settings.png`
(Read tool). Verify: full 1024×768 window, sidebar visible, correct page shown,
dark theme. If any capture shows a blank/partial page, re-run Step 1 (transient
first-paint issues are masked by the 100ms wait, but verify).

- [ ] **Step 6: Commit**

```bash
git add tests/reference_screenshots/macos docs/design/ux-modernization/current-state/macos
git commit -m "test(screenshots): regenerate macOS baselines post-2.8.x redesign (GH#224)

Adds first captures of boot_analysis, disk_tools, system_logs (+ optional
homebrew/docker) and copies all captures into the UX-modernization
evidence pack."
```

---

### Task 3: Capture Linux evidence via CI workflow

**Files:**
- Create: `docs/design/ux-modernization/current-state/linux/{dark,light}/*.png`

**Interfaces:**
- Consumes: Task 1's harness change (must be pushed first — the workflow checks out the branch).
- Produces: the Linux (x64, container) half of the evidence pack. Linux *baselines* are intentionally NOT committed (xvfb flakiness, PR #209 history) — that stays a future NEX item.

- [ ] **Step 1: Push the branch so the workflow sees Tasks 1–2**

Run: `git push`
Expected: branch updated on origin.

- [ ] **Step 2: Dispatch the baseline-regeneration workflow for linux-x64**

Run: `gh workflow run screenshot-baselines.yml --ref claude/ux-modernization-spec -f platforms=linux-x64`
Then: `gh run list --workflow=screenshot-baselines.yml --limit 1` to get the run ID.
Expected: a queued run on ref `claude/ux-modernization-spec`.

- [ ] **Step 3: Wait for completion**

Run: `gh run watch <RUN_ID> --exit-status` (takes several minutes; container is `ubuntu:26.04`).
Expected: exit 0. If the Linux leg hangs >15 min, cancel and record the failure —
fall back to capturing everything in Task 4 on the homelab instead.

- [ ] **Step 4: Download and place the artifact**

```bash
gh run download <RUN_ID> -n reference-screenshots-linux -D /tmp/nexis-linux-shots
mkdir -p docs/design/ux-modernization/current-state/linux
cp -R /tmp/nexis-linux-shots/*/dark  docs/design/ux-modernization/current-state/linux/dark
cp -R /tmp/nexis-linux-shots/*/light docs/design/ux-modernization/current-state/linux/light
```

(Inspect the artifact's internal layout with `find /tmp/nexis-linux-shots -name '*.png' | head`
first and adjust the `cp` source paths to match.)

- [ ] **Step 5: Record which pages the container produced**

Run: `ls docs/design/ux-modernization/current-state/linux/dark/`
Expected: the 15 always-present pages; `apt_source_manager.png` likely present
(container has apt); `docker.png` and `gnome_settings.png` likely absent (no
Docker daemon / GNOME in container). Note absences in the commit message — Task 4
fills them.

- [ ] **Step 6: Commit**

```bash
git add docs/design/ux-modernization/current-state/linux
git commit -m "docs(design): add Linux x64 evidence captures from CI workflow"
```

---

### Task 4: Capture gated Linux pages on the homelab

**Files:**
- Create/Modify: `docs/design/ux-modernization/current-state/linux/{dark,light}/{docker,gnome_settings,apt_source_manager}.png` (whichever the environment supports)
- Create: `docs/design/ux-modernization/current-state/CAPTURE_NOTES.md`

**Interfaces:**
- Consumes: pushed branch from Task 3.
- Produces: gated-page evidence + a gap log the audit tasks must consult.

- [ ] **Step 1: Build and capture on the homelab (Ryzen/Ubuntu 24.04)**

On the homelab (ssh):

```bash
git clone --branch claude/ux-modernization-spec https://github.com/s4solutionsllc/Nexis.git /tmp/nexis-ux && cd /tmp/nexis-ux
cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build --target test-ScreenshotTests -j$(nproc)
./scripts/update_screenshots.sh   # auto-selects QT_QPA_PLATFORM=offscreen headless (SSO-3729)
ls tests/reference_screenshots/linux/dark/
```

Expected: `Generated:` lines including `docker.png` (Docker runs Coolify on this
host) and `apt_source_manager.png`. `gnome_settings.png` will be skipped —
headless server has no GNOME.

- [ ] **Step 2: Pull the gated-page PNGs back to the Mac**

```bash
for theme in dark light; do
  scp "homelab:/tmp/nexis-ux/tests/reference_screenshots/linux/$theme/{docker,apt_source_manager}.png" \
      "docs/design/ux-modernization/current-state/linux/$theme/" || true
done
```

(Adjust host alias to the actual SSH config name. `|| true` because either file
may legitimately be absent.)

- [ ] **Step 3: Write the capture-gap log**

Create `docs/design/ux-modernization/current-state/CAPTURE_NOTES.md` recording,
for each of the 19 capture targets (18 audit pages + the Dashboard reference)
× {macos, linux}: captured (with path) /
skipped (reason) / N-A (platform). Explicitly note: `gnome_settings` has **no
live capture** — its audit and mockup derive from
`shared/nexis/Pages/GnomeSettings/*.ui` and are flagged "needs visual
verification during implementation UAT" (spec §4 risk).

- [ ] **Step 4: Commit**

```bash
git add docs/design/ux-modernization/current-state
git commit -m "docs(design): add homelab captures for gated pages + capture-gap log"
```

---

### Task 5: Build the external source pack (`sources.md`)

**Files:**
- Create: `docs/design/ux-modernization/sources.md`

**Interfaces:**
- Produces: citation keys (`[HIG-*]`, `[GNOME-*]`, `[QT-*]`, `[NNG-*]`) used by DESIGN_SYSTEM.md, every audit sheet, and work-items.md.

- [ ] **Step 1: Fetch and verify each source (WebFetch), then write `sources.md`**

For each row: fetch the URL, confirm it's live and the claim it supports is
actually on the page, record title + access date (2026-07-13 or actual). Table
format — one row per source:

```markdown
| Key | Claim it supports | URL | Title | Accessed |
|---|---|---|---|---|
| QT-QSS | QSS selector styling / re-polish semantics; prefer app-level stylesheet | https://doc.qt.io/qt-6/stylesheet.html | Qt Style Sheets | 2026-07-13 |
| QT-QSS-SYNTAX | Property-selector re-polish requirement (BUG-56 external basis) | https://doc.qt.io/qt-6/stylesheet-syntax.html | The Style Sheet Syntax | 2026-07-13 |
| QT-SHADOW | QGraphicsDropShadowEffect renders source into pixmap + blur = repaint cost; bounded-effects guardrail | https://doc.qt.io/qt-6/qgraphicsdropshadoweffect.html | QGraphicsDropShadowEffect | 2026-07-13 |
| HIG-MACOS | macOS platform conventions: typography hierarchy, materials/elevation, layout margins | https://developer.apple.com/design/human-interface-guidelines | Apple Human Interface Guidelines | 2026-07-13 |
| GNOME-HIG | Linux/GNOME conventions: boxed lists, header patterns, spacing units | https://developer.gnome.org/hig/ | GNOME Human Interface Guidelines | 2026-07-13 |
| NNG-TABLES | Data-table legibility: row density, alignment, zebra vs whitespace | https://www.nngroup.com/articles/data-tables/ | Data Tables (NN/g) | 2026-07-13 |
| NNG-EMPTY | Empty states must explain + offer next action | https://www.nngroup.com/articles/empty-state-interface-design/ | Empty-State Design (NN/g) | 2026-07-13 |
```

Add further sources ONLY if an audit task needs a claim not covered here; append,
never delete. If a URL is dead, find the current equivalent on the same
authority's domain and note the substitution.

- [ ] **Step 2: Commit**

```bash
git add docs/design/ux-modernization/sources.md
git commit -m "docs(design): add verified external source pack for UX audits"
```

---

### Task 6: Write `docs/design/DESIGN_SYSTEM.md`

**Files:**
- Create: `docs/design/DESIGN_SYSTEM.md`

**Interfaces:**
- Consumes: citation keys from Task 5.
- Produces: normative section numbers (`DS §n`) referenced by every audit sheet, mockup, and work-item draft.

- [ ] **Step 1: Write the document with this exact structure**

Sections (all constants below are pre-researched — verify each against the cited
file:line while writing; if a line number drifted, fix the citation, not the value):

```markdown
# Nexis Design System (extracted from v2.8.x Dashboard, GH#191/#213/#214)

## DS §1 Surface hierarchy
@pageContent → @cardBg → @cardBgElevated; elevated surfaces carry shadow.
Values: values.ini:2,29,30 both themes.

## DS §2 Card chrome (the elevated-card recipe)
radius 12px; border 1px @borderColor; fill @cardBgElevated; shadow alpha 90,
blur 26, offset (0,2), color @shadowColor; 8px outer margin for shadow room.
Sources: dashboard_tile_wrapper.cpp:107-130, utilities.h:30-32, style.qss:765-769.

## DS §3 Header anatomy
3px × ≥26px accent bar (radius 1) + title row + muted source line; gear/actions
pinned top-right 24×24. Content margins 14,12,14,10; spacing 6/8.
Sources: metric_tile_base.cpp:255-307, :171-177.

## DS §4 Typography scale
Title 9pt/600 @color06 · source/meta 8pt/500 @tertiaryText · hero value
14pt/700 @color05 · pill text 8pt. Source: style.qss:772-815.

## DS §5 Status & state patterns
Trend/status pill (radius 8, padding 1 6, bg @chartGridColor); [status="…"]
selectors; empty state = icon + one-line explanation + action [NNG-EMPTY];
loading state = static skeleton, no animation (guardrail).

## DS §6 Tracks, bars, charts
Track/grid color @chartGridColor (moved off @color02 in 2.8.1 — CHANGELOG);
progress bar bg @chartGridColor radius 2 max-height 4 (style.qss:817-826);
chart series @chartSeries01–20.

## DS §7 Tables & lists
Container-level cards, never per-row shadows (spec §8 risk). Row density and
alignment rules cite [NNG-TABLES]; platform variance notes cite [HIG-MACOS],
[GNOME-HIG].

## DS §8 Theming rules (binding)
79 tokens × 2 themes, parity test-enforced; BUG-47 no hardcoded colors;
BUG-49 non-substring token names; refreshThemeColors()+sigChangedAppTheme for
painted colors only; inline setStyleSheet() must resolve tokens at runtime
(tests/theme/test_theme_tokens.cpp).

## DS §9 Performance guardrails (binding)
The six guardrails from spec §6, each with citation: QSS-first [QT-QSS],
bounded shadows [QT-SHADOW] + dashboard-proven ceiling, no timers/animation,
FR-97 lazy construction, BUG-56 re-polish [QT-QSS-SYNTAX], measured
cold-start/idle-CPU acceptance.
```

Every `DS §n` heading is stable — audits and work items cite them; do not
renumber in later edits.

- [ ] **Step 2: Verify every file:line citation**

For each citation in the doc, open the file at that line and confirm the
constant. Expected: all match (they were extracted 2026-07-13 from this branch).

- [ ] **Step 3: Commit**

```bash
git add docs/design/DESIGN_SYSTEM.md
git commit -m "docs(design): add normative design system extracted from 2.8.x dashboard"
```

---

### Task 7: Audit template + table-centric page audits

**Files:**
- Create: `docs/design/ux-modernization/audits/_TEMPLATE.md`
- Create: `docs/design/ux-modernization/audits/{processes,search,system_logs,boot_analysis,hardware_info,uninstaller}.md`

**Interfaces:**
- Consumes: DESIGN_SYSTEM.md `DS §n`, evidence pack paths, `sources.md` keys.
- Produces: `_TEMPLATE.md` consumed verbatim by Tasks 8–11; audit sheets consumed by mockup Tasks 13–17 and work-items Task 18.

- [ ] **Step 1: Create `_TEMPLATE.md`**

```markdown
# <Page Name> — UX Audit

**Platforms:** <macOS / Linux / both; gating condition if any>
**Structure class:** <table-centric | list-of-rows | tree-stacked | charts | forms>
**Source files:** <page .cpp/.h/.ui paths with LOC>
**Evidence:** <current-state PNG paths, or CAPTURE_NOTES.md gap reference>
**Styling today:** <global-QSS-only | has refreshThemeColors()> (per theme-system research)

## Current state (2–4 sentences)

## Deviations from DESIGN_SYSTEM.md
| # | Area | Current | Target | DS § | Source |
|---|------|---------|--------|------|--------|
| D1 | e.g. Surfaces | flat @pageContent, no card container | section content in elevated card | §1–2 | current-state/macos/dark/<page>.png |

## Recommendations
R1. <imperative change>. Source: <file:line | screenshot path | sources.md key>
...

## Hardcoded-color / inline-stylesheet findings
Output of: grep -n "setStyleSheet\|QColor(" <page .cpp files> — each hit judged
against DS §8, or "none".

## Performance notes
Which guardrails (DS §9) this page's restyle must watch, e.g. shadow count for
long scroll areas, table repaint areas.

## Out of scope (explicitly not proposed)
```

- [ ] **Step 2: Write the six table-centric audits**

For each of Processes (`shared/nexis/Pages/Processes/`), Search, SystemLogs,
BootAnalysis, HardwareInfo, Uninstaller: read the page source + `.ui`, open its
evidence PNGs (dark+light, both platforms where present), fill the template
completely. Structure-class checklist to evaluate on every one of these pages:
table container (card vs bare), header row treatment, toolbar/filter row
anatomy vs DS §3, row density/alignment [NNG-TABLES], status coloring vs DS §5,
empty-result state [NNG-EMPTY], typography vs DS §4.

- [ ] **Step 3: Verify zero unsourced recommendations**

Run: `grep -L "Source:" docs/design/ux-modernization/audits/*.md` → expected: empty output.
Also `grep -rn "R[0-9]*\." docs/design/ux-modernization/audits/ | grep -v "Source:"` should show only lines whose following line carries the source — spot-check any hit.

- [ ] **Step 4: Commit**

```bash
git add docs/design/ux-modernization/audits
git commit -m "docs(design): add audit template + table-centric page audits"
```

---

### Task 8: List-of-rows page audits

**Files:**
- Create: `docs/design/ux-modernization/audits/{services,startup_apps,apt_source_manager}.md`

**Interfaces:**
- Consumes: `_TEMPLATE.md` (Task 7), DS §n, evidence pack, sources.md.

- [ ] **Step 1: Write the three audits** using `_TEMPLATE.md`. Pages: Services
(`shared/nexis/Pages/Services/`, 206 LOC, item rows via `service_item.ui`),
StartupApps (871 LOC + platform edit dialogs), APT Source Manager
(`linux/nexis/Pages/AptSourceManager/`, ~2,027 LOC, Linux-gated). Class
checklist: row-item chrome vs DS §2 (card-per-row is a shadow-count risk — DS
§9; recommend flat `@cardBg` rows in an elevated container), row header/label
typography vs DS §4, toggle/action affordances, list empty state [NNG-EMPTY],
GNOME boxed-list comparison [GNOME-HIG] for the Linux-only page.

- [ ] **Step 2: Source check** — same greps as Task 7 Step 3. Expected: empty.

- [ ] **Step 3: Commit**

```bash
git add docs/design/ux-modernization/audits
git commit -m "docs(design): add list-of-rows page audits"
```

---

### Task 9: Tree/stacked-tool page audits

**Files:**
- Create: `docs/design/ux-modernization/audits/{system_cleaner,disk_tools,docker,helpers,homebrew}.md`

**Interfaces:**
- Consumes: `_TEMPLATE.md`, DS §n, evidence pack, sources.md.

- [ ] **Step 1: Write the five audits.** Pages: SystemCleaner (2,079 LOC,
QStackedWidget + category trees), DiskTools (895), Docker (520, tabs+trees,
gated), Helpers (5,280 — the tool hub; audit the hub grid AND the shared tool-
widget chrome once, not per-tool), Homebrew (`macos/nexis/Pages/Homebrew/`,
497, macOS-gated). Class checklist: stacked-page navigation chrome, tree
container card treatment vs DS §2, category header anatomy vs DS §3,
scan/progress states vs DS §5–6, `QPlainTextEdit` output areas (mono font
token), per-widget setStyleSheet audit (Helpers is the biggest offender risk —
it has 3–8 inline styles per tool widget).

- [ ] **Step 2: Source check** — same greps. Expected: empty.

- [ ] **Step 3: Commit**

```bash
git add docs/design/ux-modernization/audits
git commit -m "docs(design): add tree/stacked-tool page audits"
```

---

### Task 10: Chart page audits

**Files:**
- Create: `docs/design/ux-modernization/audits/{resources,network_usage}.md`

**Interfaces:**
- Consumes: `_TEMPLATE.md`, DS §n, evidence pack, sources.md.

- [ ] **Step 1: Write the two audits.** Resources (2,421 LOC, QChart history
charts + disk-usage launcher; has `refreshThemeColors`), Network (951 LOC,
custom `BarChartWidget`). Class checklist: chart container cards vs DS §2,
chart surface tokens vs DS §6 (`@chartBackgroundColor`, `@chartGridColor`,
series palette), chart title/legend typography vs DS §4, launcher-widget
chrome, no animation additions (DS §9).

- [ ] **Step 2: Source check** — same greps. Expected: empty.

- [ ] **Step 3: Commit**

```bash
git add docs/design/ux-modernization/audits
git commit -m "docs(design): add chart page audits"
```

---

### Task 11: Form page audits

**Files:**
- Create: `docs/design/ux-modernization/audits/{settings,gnome_settings}.md`

**Interfaces:**
- Consumes: `_TEMPLATE.md`, DS §n, evidence pack, sources.md, CAPTURE_NOTES.md.

- [ ] **Step 1: Write the two audits.** Settings (857 LOC, scroll-area form
sections; has `refreshThemeColors` + `addDropShadow` at settings_page.cpp:213,768
— compare its shadow params against the DS §2 recipe and flag drift), and
GnomeSettings (1,228 LOC, Linux-gated, **no live capture** — derive from the
page + 4 tab `.ui` files, mark every visual claim `Source: <file>.ui` and add
the "needs UAT visual verification" banner per CAPTURE_NOTES.md). Class
checklist: section grouping into cards vs DS §2–3, label/field typography vs
DS §4, control alignment [HIG-MACOS]/[GNOME-HIG], destructive-action styling
(`@destructiveColor`) vs DS §5.

- [ ] **Step 2: Source check** — same greps. Expected: empty.

- [ ] **Step 3: Commit**

```bash
git add docs/design/ux-modernization/audits
git commit -m "docs(design): add form page audits"
```

---

### Task 12: Mockup scaffold (tokens, base styles, renderer)

**Files:**
- Create: `scripts/design/tokens_to_css.py`
- Create: `scripts/design/render_mockups.sh`
- Create: `docs/design/ux-modernization/mockups/base.css`
- Create: `docs/design/ux-modernization/mockups/pages/_smoke.html`

**Interfaces:**
- Produces: `tokens.css` (generated), `base.css` classes (`.card`, `.section-header`, `.accent-bar`, `.pill`, `.t-title`, `.t-meta`, `.t-hero`), and `render_mockups.sh` — consumed by Tasks 13–17.

- [ ] **Step 1: Write `scripts/design/tokens_to_css.py`**

```python
#!/usr/bin/env python3
"""Generate mockups/tokens.css from the two theme values.ini files.

values.ini is a flat `@key=#hex` file (no INI sections). Dark theme lives in
themes/default/, light in themes/light/ — same key set, test-enforced.
"""
import pathlib

ROOT = pathlib.Path(__file__).resolve().parents[2]
THEMES = {"dark": "default", "light": "light"}
OUT = ROOT / "docs/design/ux-modernization/mockups/tokens.css"


def read_tokens(theme_dir: str) -> dict:
    ini = ROOT / f"shared/nexis/static/themes/{theme_dir}/style/values.ini"
    tokens = {}
    for line in ini.read_text().splitlines():
        line = line.strip()
        if not line.startswith("@") or "=" not in line:
            continue
        key, value = line[1:].split("=", 1)
        tokens[key] = value
    return tokens


blocks = ["/* GENERATED by scripts/design/tokens_to_css.py — do not edit. */\n"]
for scheme, theme_dir in THEMES.items():
    tokens = read_tokens(theme_dir)
    body = "\n".join(f"  --{k}: {v};" for k, v in sorted(tokens.items()))
    selector = ":root" if scheme == "dark" else ':root[data-theme="light"]'
    blocks.append(f"{selector} {{\n{body}\n}}\n")

OUT.parent.mkdir(parents=True, exist_ok=True)
OUT.write_text("\n".join(blocks))
print(f"Wrote {OUT}")
```

- [ ] **Step 2: Generate and verify `tokens.css`**

Run: `python3 scripts/design/tokens_to_css.py && grep -c -- "--" docs/design/ux-modernization/mockups/tokens.css`
Expected: `Wrote …/tokens.css` and a count of **158** custom properties (79 × 2 themes).

- [ ] **Step 3: Write `base.css`** (the DS recipes as CSS; Qt pt ≈ px at macOS 72dpi)

```css
/* Nexis mockup base — encodes DESIGN_SYSTEM.md recipes. tokens.css first. */
* { box-sizing: border-box; margin: 0; }
body {
  background: var(--pageContent);
  color: var(--color05);
  font: 500 12px/1.45 -apple-system, "Segoe UI", "Noto Sans", sans-serif;
}
/* DS §2 — elevated card: dashboard_tile_wrapper.cpp:107-130, addDropShadow(90,26) */
.card {
  background: var(--cardBgElevated);
  border: 1px solid var(--borderColor);
  border-radius: 12px;
  box-shadow: 0 2px 26px rgba(0, 0, 0, 0.35); /* alpha 90/255 */
  margin: 8px;                                 /* shadow room */
  padding: 14px 12px 10px 14px;                /* root margins, rotated to CSS order T R B L = 12,14,10,14 */
}
.card--flat { box-shadow: none; background: var(--cardBg); }
/* DS §3 — header anatomy: metric_tile_base.cpp:255-307 */
.section-header { display: flex; gap: 8px; align-items: flex-start; }
.accent-bar { width: 3px; min-height: 26px; border-radius: 1px; background: var(--accentColor); flex: none; }
/* DS §4 — typography scale: style.qss:772-815 */
.t-title { font-size: 9pt; font-weight: 600; color: var(--color06); }
.t-meta  { font-size: 8pt; font-weight: 500; color: var(--tertiaryText); }
.t-hero  { font-size: 14pt; font-weight: 700; color: var(--color05); }
/* DS §5 — pill */
.pill {
  display: inline-block; font-size: 8pt; padding: 1px 6px;
  border-radius: 8px; background: var(--chartGridColor); color: var(--tertiaryText);
}
```

Note the padding comment: `metric_tile_base.cpp:257` sets Qt margins
(L,T,R,B)=(14,12,14,10); CSS shorthand is T R B L. Keep both notations visible
so implementing agents don't transpose.

- [ ] **Step 4: Write `render_mockups.sh`**

```bash
#!/usr/bin/env bash
# Render every mockup page to dark+light PNGs at the harness capture size.
set -euo pipefail
cd "$(dirname "$0")/../../docs/design/ux-modernization/mockups"
mkdir -p renders
for f in pages/*.html; do
  name="$(basename "$f" .html)"
  npx --yes playwright screenshot --browser=chromium \
    --viewport-size=1024,768 "file://$PWD/$f" "renders/${name}_dark.png"
  npx --yes playwright screenshot --browser=chromium \
    --viewport-size=1024,768 "file://$PWD/$f?theme=light" "renders/${name}_light.png"
done
echo "Rendered $(ls renders/*.png | wc -l | tr -d ' ') PNGs"
```

Make executable: `chmod +x scripts/design/render_mockups.sh scripts/design/tokens_to_css.py`
One-time browser install: `npx --yes playwright install chromium`

- [ ] **Step 5: Write the smoke-test page `pages/_smoke.html`**

```html
<!doctype html>
<meta charset="utf-8">
<title>smoke</title>
<link rel="stylesheet" href="../tokens.css">
<link rel="stylesheet" href="../base.css">
<script>
  const t = new URLSearchParams(location.search).get("theme");
  if (t) document.documentElement.dataset.theme = t;
</script>
<div class="card" style="width:320px">
  <div class="section-header">
    <div class="accent-bar" style="background:var(--cpuColor)"></div>
    <div><div class="t-title">CPU</div><div class="t-meta">Apple M2 · 8 cores</div></div>
  </div>
  <div class="t-hero" style="margin-top:12px">42%</div>
  <span class="pill">→ stable</span>
</div>
```

- [ ] **Step 6: Render and verify**

Run: `./scripts/design/render_mockups.sh`
Expected: `Rendered 2 PNGs`. Open `renders/_smoke_dark.png` and
`renders/_smoke_light.png` (Read tool): dark shows a warm-dark elevated card
(`#32343A`) on `#1A1C22`; light shows `#FFF8F2` on `#F5F0EB`; accent bar uses
`@cpuColor`. If both PNGs look identical, the `?theme=light` query isn't
reaching the script — check the `<script>` block placement.

- [ ] **Step 7: Commit**

```bash
git add scripts/design docs/design/ux-modernization/mockups
git commit -m "docs(design): add mockup scaffold — token CSS generator, base styles, renderer"
```

---

### Task 13: Table-centric page mockups

**Files:**
- Create: `docs/design/ux-modernization/mockups/pages/{processes,search,system_logs,boot_analysis,hardware_info,uninstaller}.html`
- Create: corresponding `renders/*_{dark,light}.png`

**Interfaces:**
- Consumes: `tokens.css`/`base.css` classes and `render_mockups.sh` (Task 12); audit recommendations (Task 7).

- [ ] **Step 1: Build the six mockups.** Each page: reproduce the current layout
skeleton (sidebar strip + page area, from the evidence PNG) and apply exactly
the audit's R-numbered recommendations — nothing more (YAGNI: a mockup that
"improves" beyond its audit is a plan violation). Use realistic data copied
from the evidence screenshots. Annotate each region with an HTML comment naming
the audit rec it implements: `<!-- R3: table in .card container, DS §2 -->`.

- [ ] **Step 2: Render** — `./scripts/design/render_mockups.sh`
Expected: 2 PNGs per page added; visually compare each dark render side-by-side
with `current-state/macos/dark/<page>.png` — layout skeleton must match, only
chrome differs.

- [ ] **Step 3: Commit**

```bash
git add docs/design/ux-modernization/mockups
git commit -m "docs(design): add table-centric page mockups"
```

---

### Task 14: List-of-rows page mockups

**Files:**
- Create: `mockups/pages/{services,startup_apps,apt_source_manager}.html` + renders

**Interfaces:** Consumes Task 12 scaffold + Task 8 audits.

- [ ] **Step 1: Build the three mockups** (same rules as Task 13 Step 1; APT page uses the Linux evidence PNGs as its skeleton reference).
- [ ] **Step 2: Render and compare** — same as Task 13 Step 2.
- [ ] **Step 3: Commit** — `git add docs/design/ux-modernization/mockups && git commit -m "docs(design): add list-of-rows page mockups"`

---

### Task 15: Tree/stacked-tool page mockups

**Files:**
- Create: `mockups/pages/{system_cleaner,disk_tools,docker,helpers,homebrew}.html` + renders

**Interfaces:** Consumes Task 12 scaffold + Task 9 audits.

- [ ] **Step 1: Build the five mockups** (Helpers: mock the hub grid + ONE representative tool widget, per the audit's single-chrome decision).
- [ ] **Step 2: Render and compare** — same as Task 13 Step 2.
- [ ] **Step 3: Commit** — `git add docs/design/ux-modernization/mockups && git commit -m "docs(design): add tree/stacked-tool page mockups"`

---

### Task 16: Chart page mockups

**Files:**
- Create: `mockups/pages/{resources,network_usage}.html` + renders

**Interfaces:** Consumes Task 12 scaffold + Task 10 audits.

- [ ] **Step 1: Build the two mockups** (charts as static SVG/CSS shapes using `--chartSeries01…`, `--chartGridColor`, `--chartBackgroundColor`; no JS chart libs).
- [ ] **Step 2: Render and compare** — same as Task 13 Step 2.
- [ ] **Step 3: Commit** — `git add docs/design/ux-modernization/mockups && git commit -m "docs(design): add chart page mockups"`

---

### Task 17: Form page mockups

**Files:**
- Create: `mockups/pages/{settings,gnome_settings}.html` + renders

**Interfaces:** Consumes Task 12 scaffold + Task 11 audits.

- [ ] **Step 1: Build the two mockups** (GnomeSettings skeleton derives from its `.ui` files — include the `<!-- UNVERIFIED: no live capture, see CAPTURE_NOTES.md -->` banner comment at the top).
- [ ] **Step 2: Render and compare** (Settings against evidence; GnomeSettings against the `.ui` structure only).
- [ ] **Step 3: Commit** — `git add docs/design/ux-modernization/mockups && git commit -m "docs(design): add form page mockups"`

---

### Task 18: Paperclip work-item drafts (`work-items.md`)

**Files:**
- Create: `docs/design/ux-modernization/work-items.md`

**Interfaces:**
- Consumes: everything — DS §n, audits (R-numbers), mockup paths, CAPTURE_NOTES.md, spec §6/§7.

- [ ] **Step 1: Write the Phase-1 foundation items** using this skeleton for every item in the file:

```markdown
### <working title>  (Paperclip: create as NEX-___)
**Phase:** 1-Foundation | 2-Page
**Blocked by:** <item titles or "—">
**Files:** <exact paths>
**Design refs:** DS §<n>…; audit <page>.md R<n>…; mockup pages/<page>.html + renders/<page>_{dark,light}.png
**Change summary:** <3–6 bullets, imperative>
**Acceptance criteria:**
- Visual match to mockup within screenshot tolerance (fuzz 8 / 1.0%), dark + light
- Guardrails DS §9: no new timers/animation; shadow count ≤ <n>; QSS-first; FR-97 intact
- Cold-start and idle CPU unchanged within noise (macOS + Linux)
- Screenshot baselines regenerated for touched pages
- CHANGELOG.md + docs/APPLICATION_OVERVIEW.md updated
**UAT (business-user steps):** <numbered tap/navigate/verify steps>
```

Phase-1 items (5): F1 shared card container (widget + QSS class per DS §2);
F2 section-header pattern (DS §3); F3 state patterns — empty/loading/status pill
(DS §5); F4 QSS consolidation groundwork (move audit-flagged inline styles into
the master template); F5 Linux screenshot baselines committed via CI workflow
(the deferred half of Task 3).

- [ ] **Step 2: Write the 18 Phase-2 page items** — one per audited page (all
audits from Tasks 7–11), each blocked by F1–F4, ordered: Processes, Resources,
SystemCleaner first (high-traffic, spec §7); BootAnalysis, GnomeSettings last.
Every item fully self-contained per the skeleton — an agent must be able to
implement from the item + referenced artifacts without this conversation.

- [ ] **Step 3: Cross-check completeness**

Run: `ls docs/design/ux-modernization/audits/*.md | grep -v _TEMPLATE | wc -l` → expected 18;
`grep -c "^### " docs/design/ux-modernization/work-items.md` → expected 23 (5 + 18).
Every audit file name must appear in work-items.md: verify with
`for f in docs/design/ux-modernization/audits/*.md; do n=$(basename $f .md); [ "$n" = "_TEMPLATE" ] && continue; grep -q "$n" docs/design/ux-modernization/work-items.md || echo "MISSING: $n"; done` → no output.

- [ ] **Step 4: Commit**

```bash
git add docs/design/ux-modernization/work-items.md
git commit -m "docs(design): add Paperclip work-item drafts for UX modernization"
```

---

### Task 19: Package review, spec acceptance check, PR

**Files:**
- Modify: `docs/superpowers/specs/2026-07-13-ux-modernization-design.md` (tick §9 acceptance boxes)

**Interfaces:** Consumes the whole package.

- [ ] **Step 1: Run the spec §9 acceptance checklist** against the actual tree —
each box only ticked with the verifying command output in hand:
constants traceable (spot-check 5 citations in DESIGN_SYSTEM.md), evidence pack
coverage vs CAPTURE_NOTES.md, `grep -L "Source:"` audit check, mockup count
(`ls mockups/renders/*.png | wc -l` ≥ 36 = 18 pages × 2 themes), work-items
cross-check (Task 18 Step 3), and `git diff native --stat -- shared/ linux/ macos/`
→ empty (no app source touched).

- [ ] **Step 2: Run the full test suite once**

Run: `ctest --test-dir build --output-on-failure`
Expected: all tests pass (the only code change is the harness; theme-token tests
prove no token/app drift).

- [ ] **Step 3: Push and open the PR**

```bash
git push
gh pr create --title "docs(design): UX modernization planning package + full screenshot coverage" \
  --body "$(cat <<'EOF'
## Summary
- Extends the screenshot harness to all 19 pages (optional-page handling for gated pages) and regenerates macOS baselines (GH#224)
- Adds the UX-modernization planning package per docs/superpowers/specs/2026-07-13-ux-modernization-design.md: DESIGN_SYSTEM.md, full evidence pack (macOS local, Linux CI + homelab), 18 sourced page audits, 18 HTML+PNG mockups, 23 Paperclip work-item drafts

## Test plan
- [ ] ctest full suite green (incl. ScreenshotTests compare mode on macOS)
- [ ] Spec §9 acceptance checklist ticked with evidence

🤖 Generated with [Claude Code](https://claude.com/claude-code)
EOF
)"
```

Expected: PR URL printed. Report it to Luke. **Do not merge.**

- [ ] **Step 4: Final commit for the ticked spec checklist**

```bash
git add docs/superpowers/specs/2026-07-13-ux-modernization-design.md
git commit -m "docs(design): tick spec acceptance criteria after package review"
git push
```
