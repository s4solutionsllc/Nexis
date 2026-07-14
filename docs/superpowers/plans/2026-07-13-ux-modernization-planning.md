# UX Modernization Prototype Package — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Produce before/after image prototypes of every Nexis page restyled in the
v2.8.x dashboard design language, get them approved page-by-page by Luke via a review
gallery, and package approved pages into Paperclip work-item drafts — per
`docs/superpowers/specs/2026-07-13-ux-modernization-design.md`.

**Architecture:** Zero app/test code changes. Current-state screenshots are harvested
from the existing screenshot test's output directory (12 pages, no modification) and by
running the built app for the rest. Prototypes are token-true HTML rendered to PNG. The
maintainer approval gate sits between prototyping and work-item drafting.

**Tech Stack:** existing `test-ScreenshotTests` binary (run as-is), macOS `screencapture`,
Python 3 (token→CSS), Playwright via `npx` (rendering), private Claude artifact (gallery).

## Global Constraints

- Branch: `claude/ux-modernization-spec` (default branch is `native`; never commit/push to it).
- **No changes under `shared/`, `linux/`, `macos/`, or `tests/` — verify with `git status` before every commit.** New files only under `docs/design/`, `docs/superpowers/`, `scripts/design/`.
- Committed screenshot baselines (`tests/reference_screenshots/`) must be byte-identical at the end of every task (`git diff --stat tests/` → empty).
- Prototypes use only colors from `shared/nexis/static/themes/{default,light}/style/values.ini` via generated `tokens.css`.
- Every rationale note claim carries a source: repo `file:line`, evidence PNG path, or `sources.md` key.
- Performance guardrails (spec §5) are binding content for DESIGN_SYSTEM.md and all work-item drafts.
- Conventional commits. Do not merge the PR — Luke reviews.
- Deliverable layout: `docs/design/DESIGN_SYSTEM.md`; under `docs/design/ux-modernization/`: `current-state/`, `sources.md`, `mockups/{tokens.css,base.css,pages/,renders/,notes/}`, `review-gallery.html`, `work-items.md`.

---

### Task 1: Harvest current-state captures on macOS (no code changes)

**Files:**
- Create: `docs/design/ux-modernization/current-state/macos/{dark,light}/*.png`
- Create: `docs/design/ux-modernization/current-state/CAPTURE_NOTES.md` (started here, finished Task 2)

**Interfaces:**
- Produces: before-PNGs for all prototype and gallery tasks. Naming: snake_case page names matching the harness (`dashboard`, `processes`, …) plus `boot_analysis`, `disk_tools`, `system_logs`, `homebrew`, `docker` for manual captures.

- [ ] **Step 1: Build (if needed) and run the screenshot test once, harvesting its outputs**

```bash
cmake --build build --target test-ScreenshotTests -j$(sysctl -n hw.ncpu)
ctest --test-dir build -R ScreenshotTests --output-on-failure || true
ls build/test_screenshots/macos/dark/
```

Expected: 12 PNGs per theme under `build/test_screenshots/macos/{dark,light}/`
(the binary saves every capture before comparing — failures, e.g. the stale
dashboard baselines, are irrelevant here, hence `|| true`).

- [ ] **Step 2: Copy into the evidence pack**

```bash
mkdir -p docs/design/ux-modernization/current-state/macos/{dark,light}
cp build/test_screenshots/macos/dark/*.png  docs/design/ux-modernization/current-state/macos/dark/
cp build/test_screenshots/macos/light/*.png docs/design/ux-modernization/current-state/macos/light/
```

- [ ] **Step 3: Capture the pages the harness doesn't cover, by running the app**

Build and launch the real app (`cmake --build build -j…`, binary under
`build/output/`). For each of: Boot Analysis, Disk Tools, System Logs, plus
Docker and Homebrew if their sidebar buttons are present — navigate to the page
and capture the window:

```bash
# Window ID of the frontmost Nexis window, then capture without shadow:
osascript -e 'tell app "Nexis" to activate'
screencapture -o -l $(osascript -e 'tell app "System Events" to tell process "Nexis" to get value of attribute "AXWindowNumber" of window 1') \
  docs/design/ux-modernization/current-state/macos/dark/<page>.png
```

Resize the window to ~1024×768 first so captures match the harness framing.
Then switch the theme to Light on the Settings page and repeat the captures
into `.../light/`. (If AppleScript window-ID lookup fails, use interactive
`screencapture -o -w` and click the window.)

- [ ] **Step 4: Verify the macOS set and confirm the repo is clean**

```bash
ls docs/design/ux-modernization/current-state/macos/dark/ | wc -l   # expected ≥ 15
diff <(ls docs/design/ux-modernization/current-state/macos/dark) \
     <(ls docs/design/ux-modernization/current-state/macos/light)    # expected: empty
git status --short -- shared/ linux/ macos/ tests/                   # expected: empty
```

Spot-open three PNGs (Read tool): correct page, correct theme, full window.

- [ ] **Step 5: Start `CAPTURE_NOTES.md`** — table of 19 pages (18 prototype pages + Dashboard reference) × platform: captured (path) / pending-linux / not-capturable (reason).

- [ ] **Step 6: Commit**

```bash
git add docs/design/ux-modernization/current-state
git commit -m "docs(design): add macOS current-state captures for UX prototypes"
```

---

### Task 2: Harvest Linux captures on the homelab (throwaway clone, nothing committed there)

**Files:**
- Create: `docs/design/ux-modernization/current-state/linux/{dark,light}/*.png`
- Modify: `docs/design/ux-modernization/current-state/CAPTURE_NOTES.md`

**Interfaces:**
- Consumes: nothing from Task 1 (parallel-safe except CAPTURE_NOTES.md).
- Produces: Linux before-PNGs; final capture-gap log for GnomeSettings/APT.

- [ ] **Step 1: Generate captures on the homelab in a throwaway clone**

On the homelab (ssh; headless-safe via offscreen QPA, SSO-3729):

```bash
git clone --depth 1 --branch claude/ux-modernization-spec https://github.com/s4solutionsllc/Nexis.git /tmp/nexis-ux && cd /tmp/nexis-ux
cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build --target test-ScreenshotTests -j$(nproc)
./scripts/update_screenshots.sh
ls tests/reference_screenshots/linux/dark/
```

Expected: 12 PNGs per theme. This writes into the **clone's** working tree only
— it is deleted afterwards and nothing is committed from the homelab.

- [ ] **Step 2: Pull the captures back and clean up**

```bash
mkdir -p docs/design/ux-modernization/current-state/linux/{dark,light}
scp 'homelab:/tmp/nexis-ux/tests/reference_screenshots/linux/dark/*.png'  docs/design/ux-modernization/current-state/linux/dark/
scp 'homelab:/tmp/nexis-ux/tests/reference_screenshots/linux/light/*.png' docs/design/ux-modernization/current-state/linux/light/
ssh homelab 'rm -rf /tmp/nexis-ux'
```

(Adjust `homelab` to the actual SSH alias.)

- [ ] **Step 3: Finish `CAPTURE_NOTES.md`** — record: Linux pages captured; pages
with **no live capture anywhere** (expected: `gnome_settings` — headless server
has no GNOME; `apt_source_manager`, `boot_analysis`, `disk_tools`, `system_logs`,
`docker` on Linux — outside the harness's 12 and not manually capturable
headless). For each: prototype derives from macOS capture of the same shared
page where one exists (BootAnalysis/DiskTools/SystemLogs/Docker are
cross-platform), else from `.ui` files (GnomeSettings, APT) with an UNVERIFIED
banner.

- [ ] **Step 4: Commit**

```bash
git add docs/design/ux-modernization/current-state
git commit -m "docs(design): add Linux current-state captures + capture-gap log"
```

---

### Task 3: Source pack (`sources.md`)

**Files:**
- Create: `docs/design/ux-modernization/sources.md`

**Interfaces:**
- Produces: citation keys `[QT-QSS]`, `[QT-QSS-SYNTAX]`, `[QT-SHADOW]`, `[HIG-MACOS]`, `[GNOME-HIG]`, `[NNG-TABLES]`, `[NNG-EMPTY]` used by DESIGN_SYSTEM.md and rationale notes.

- [ ] **Step 1: Fetch, verify, and record each source** (WebFetch each URL; confirm the supporting content; record title + access date):

```markdown
| Key | Claim it supports | URL |
|---|---|---|
| QT-QSS | QSS selector styling; prefer app-level stylesheet | https://doc.qt.io/qt-6/stylesheet.html |
| QT-QSS-SYNTAX | Property-selector re-polish requirement (BUG-56 basis) | https://doc.qt.io/qt-6/stylesheet-syntax.html |
| QT-SHADOW | Drop-shadow effect = pixmap render + blur, repaint cost | https://doc.qt.io/qt-6/qgraphicsdropshadoweffect.html |
| HIG-MACOS | macOS typography hierarchy, materials/elevation, margins | https://developer.apple.com/design/human-interface-guidelines |
| GNOME-HIG | GNOME boxed lists, header patterns, spacing units | https://developer.gnome.org/hig/ |
| NNG-TABLES | Data-table density, alignment, zebra vs whitespace | https://www.nngroup.com/articles/data-tables/ |
| NNG-EMPTY | Empty states explain + offer next action | https://www.nngroup.com/articles/empty-state-interface-design/ |
```

Add `Title` and `Accessed` columns from the fetches. Append further sources only
if a rationale note needs an uncovered claim; if a URL is dead, substitute the
current equivalent on the same authority's domain and note it.

- [ ] **Step 2: Commit** — `git add docs/design/ux-modernization/sources.md && git commit -m "docs(design): add verified external source pack"`

---

### Task 4: Write `docs/design/DESIGN_SYSTEM.md`

**Files:**
- Create: `docs/design/DESIGN_SYSTEM.md`

**Interfaces:**
- Consumes: Task 3 citation keys.
- Produces: stable section anchors `DS §1`–`DS §9` cited by every rationale note and work item.

- [ ] **Step 1: Write the document** with these sections (constants pre-researched
2026-07-13; verify each cited file:line while writing — if a line drifted, fix
the citation, not the value):

- **DS §1 Surface hierarchy** — `@pageContent` → `@cardBg` → `@cardBgElevated`; elevated surfaces carry shadow (values.ini:2,29,30 both themes).
- **DS §2 Card chrome** — radius 12px; 1px `@borderColor`; fill `@cardBgElevated`; shadow alpha 90 / blur 26 / offset (0,2) / `@shadowColor`; 8px outer margin (dashboard_tile_wrapper.cpp:107-130, utilities.h:30-32, style.qss:765-769).
- **DS §3 Header anatomy** — 3px×≥26px accent bar r1 + title row + muted source line; actions pinned top-right 24×24; margins 14,12,14,10, spacing 6/8 (metric_tile_base.cpp:255-307, :171-177).
- **DS §4 Typography scale** — title 9pt/600 `@color06` · meta 8pt/500 `@tertiaryText` · hero 14pt/700 `@color05` · pill 8pt (style.qss:772-815).
- **DS §5 Status & state patterns** — pill (r8, padding 1 6, bg `@chartGridColor`); `[status="…"]` selectors; empty state = icon + explanation + action [NNG-EMPTY]; loading = static skeleton, no animation.
- **DS §6 Tracks, bars, charts** — track/grid `@chartGridColor` (2.8.1 change); progress bar bg `@chartGridColor` r2 max-height 4 (style.qss:817-826); series `@chartSeries01–20`.
- **DS §7 Tables & lists** — container-level cards, never per-row shadows; density/alignment [NNG-TABLES]; platform notes [HIG-MACOS][GNOME-HIG].
- **DS §8 Theming rules** — 79×2 tokens, parity test-enforced; BUG-47; BUG-49; `refreshThemeColors()` only for painted colors; inline styles resolve tokens at runtime (tests/theme/test_theme_tokens.cpp).
- **DS §9 Performance guardrails** — the six from spec §5, cited: QSS-first [QT-QSS], bounded shadows [QT-SHADOW], no timers/animation, FR-97, BUG-56 [QT-QSS-SYNTAX], measured cold-start/idle-CPU.

- [ ] **Step 2: Verify every file:line citation** by opening each cited location. Expected: all match.
- [ ] **Step 3: Commit** — `git add docs/design/DESIGN_SYSTEM.md && git commit -m "docs(design): add normative design system extracted from 2.8.x dashboard"`

---

### Task 5: Prototype scaffold (tokens, base styles, renderer)

**Files:**
- Create: `scripts/design/tokens_to_css.py`, `scripts/design/render_mockups.sh`
- Create: `docs/design/ux-modernization/mockups/base.css`, `mockups/pages/_smoke.html`

**Interfaces:**
- Produces: generated `mockups/tokens.css`; `base.css` classes `.card`, `.card--flat`, `.section-header`, `.accent-bar`, `.pill`, `.t-title`, `.t-meta`, `.t-hero`; `render_mockups.sh` — consumed by Tasks 6–10.

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

- [ ] **Step 2: Generate and verify**

Run: `python3 scripts/design/tokens_to_css.py && grep -c -- "--" docs/design/ux-modernization/mockups/tokens.css`
Expected: `Wrote …/tokens.css`, count **158** (79 tokens × 2 themes).

- [ ] **Step 3: Write `base.css`** (DS recipes as CSS; Qt pt ≈ px at macOS 72dpi)

```css
/* Nexis mockup base — encodes DESIGN_SYSTEM.md recipes. Load tokens.css first. */
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
  padding: 12px 14px 10px 14px;                /* Qt margins L,T,R,B = 14,12,14,10 → CSS T,R,B,L */
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

- [ ] **Step 4: Write `scripts/design/render_mockups.sh`**

```bash
#!/usr/bin/env bash
# Render every mockup page to dark+light PNGs at the capture size.
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

Then: `chmod +x scripts/design/*.py scripts/design/*.sh` and one-time
`npx --yes playwright install chromium`.

- [ ] **Step 5: Write the smoke page `mockups/pages/_smoke.html`**

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
Expected: `Rendered 2 PNGs`. Open both renders: dark card `#32343A` on `#1A1C22`;
light card `#FFF8F2` on `#F5F0EB`; if identical, the theme query isn't applying —
check the `<script>` placement.

- [ ] **Step 7: Commit** — `git add scripts/design docs/design/ux-modernization/mockups && git commit -m "docs(design): add prototype scaffold — token CSS generator, base styles, renderer"`

---

### Task 6: Prototypes — table-centric pages (6)

**Files:**
- Create: `mockups/pages/{processes,search,system_logs,boot_analysis,hardware_info,uninstaller}.html`, matching `renders/*_{dark,light}.png`, and `mockups/notes/<page>.md` per page

**Interfaces:**
- Consumes: Task 5 scaffold classes/renderer; evidence PNGs; DS §; sources.md keys.
- Produces: prototype pairs + rationale notes consumed by the gallery (Task 11) and work items (Task 12).

- [ ] **Step 1: For each page, write the rationale note first** (`mockups/notes/<page>.md`):

```markdown
# <Page> — prototype rationale
**Before:** current-state/<platform>/dark/<page>.png
**Changes:**
1. <change>. Source: <file:line | evidence path | sources.md key>. DS §<n>.
...
**Explicitly unchanged:** <layout/interactions kept as-is>
```

Keep to the change classes Luke named: shadow depth (DS §2), surface texturing
(`@cardBg`/`@cardBgElevated` hierarchy, DS §1), layout consistency (header
anatomy DS §3, typography DS §4, spacing). Table-specific checklist: table in an
elevated container card (never per-row shadows, DS §7), toolbar/filter row using
header anatomy, status colors via DS §5, empty state [NNG-EMPTY].

- [ ] **Step 2: Build each HTML prototype** — reproduce the page's current layout
skeleton from its evidence PNG (sidebar strip + content area, realistic data
copied from the screenshot), apply exactly the note's numbered changes, nothing
more. Annotate regions: `<!-- change 3: DS §2 card container -->`.

- [ ] **Step 3: Render and compare** — `./scripts/design/render_mockups.sh`; open each
dark render next to its before-PNG: layout skeleton must match, only chrome differs.

- [ ] **Step 4: Commit** — `git add docs/design/ux-modernization/mockups && git commit -m "docs(design): add table-centric page prototypes"`

---

### Task 7: Prototypes — list-of-rows pages (3)

**Files:** `mockups/pages/{services,startup_apps,apt_source_manager}.html` + renders + notes

Same steps as Task 6. Class checklist: rows as flat `.card--flat` inside one
elevated container (shadow-count guardrail DS §9), row typography DS §4, toggle/
action affordances unchanged, empty state [NNG-EMPTY], [GNOME-HIG] boxed-list
comparison for APT (Linux-only; skeleton from Linux evidence or `.ui` per
CAPTURE_NOTES.md — include `<!-- UNVERIFIED: no live capture -->` banner if so).

- [ ] **Step 1: Rationale notes** (Task 6 Step 1 format)
- [ ] **Step 2: HTML prototypes**
- [ ] **Step 3: Render and compare**
- [ ] **Step 4: Commit** — `git commit -m "docs(design): add list-of-rows page prototypes"`

---

### Task 8: Prototypes — tree/stacked-tool pages (5)

**Files:** `mockups/pages/{system_cleaner,disk_tools,docker,helpers,homebrew}.html` + renders + notes

Same steps as Task 6. Class checklist: tree containers in DS §2 cards, category
headers per DS §3, scan/progress states DS §5–6, `QPlainTextEdit` output areas
on `@cardBg` with mono font, Helpers = hub grid + ONE representative tool widget.

- [ ] **Step 1: Rationale notes**
- [ ] **Step 2: HTML prototypes**
- [ ] **Step 3: Render and compare**
- [ ] **Step 4: Commit** — `git commit -m "docs(design): add tree/stacked-tool page prototypes"`

---

### Task 9: Prototypes — chart pages (2)

**Files:** `mockups/pages/{resources,network_usage}.html` + renders + notes

Same steps as Task 6. Charts as static SVG/CSS using `--chartSeries01…`,
`--chartGridColor`, `--chartBackgroundColor` (DS §6); chart containers as DS §2
cards; no JS chart libraries; no animation (DS §9).

- [ ] **Step 1: Rationale notes**
- [ ] **Step 2: HTML prototypes**
- [ ] **Step 3: Render and compare**
- [ ] **Step 4: Commit** — `git commit -m "docs(design): add chart page prototypes"`

---

### Task 10: Prototypes — form pages (2)

**Files:** `mockups/pages/{settings,gnome_settings}.html` + renders + notes

Same steps as Task 6. Form sections grouped into DS §2 cards with DS §3 headers;
label/field typography DS §4; control alignment [HIG-MACOS]/[GNOME-HIG];
destructive actions `--destructiveColor` (DS §5). Settings: compare its existing
`addDropShadow` params (settings_page.cpp:213,768) against DS §2 in the note.
GnomeSettings: skeleton from its `.ui` files, UNVERIFIED banner per CAPTURE_NOTES.md.

- [ ] **Step 1: Rationale notes**
- [ ] **Step 2: HTML prototypes**
- [ ] **Step 3: Render and compare**
- [ ] **Step 4: Commit** — `git commit -m "docs(design): add form page prototypes"`

---

### Task 11: Review gallery + Luke's page-by-page approval  ⛔ APPROVAL GATE

**Files:**
- Create: `docs/design/ux-modernization/review-gallery.html`
- Modify: `mockups/notes/<page>.md` (approval status lines)

**Interfaces:**
- Consumes: all 18 prototype pairs, before-PNGs, notes.
- Produces: per-page approval status (`Approved` / `Revise: <feedback>` / `Deferred`) required by Task 12.

- [ ] **Step 1: Build `review-gallery.html`** — one section per page: page name,
before (current-state PNG) and after (render) side-by-side for dark, then light;
the rationale note's change list; images embedded as `data:` URIs so the page is
self-contained. Wide rows scroll in their own container.

- [ ] **Step 2: Publish as a private artifact** (Artifact tool; favicon 🎨, stable
across revisions) and give Luke the link with clear instructions: reply per page
— approve / revise (with what's wrong) / defer.

- [ ] **Step 3: STOP. Wait for Luke's feedback.** Do not proceed to Task 12.

- [ ] **Step 4: Revision loop** — for each "revise" page: update the HTML + note,
re-render, redeploy the same artifact URL, re-request review. Repeat until every
page is Approved or Deferred. Record final status + date at the top of each
page's note.

- [ ] **Step 5: Commit** — `git add docs/design/ux-modernization && git commit -m "docs(design): add review gallery + record prototype approval outcomes"`

---

### Task 12: Paperclip work-item drafts (`work-items.md`) — approved pages only

**Files:**
- Create: `docs/design/ux-modernization/work-items.md`

**Interfaces:**
- Consumes: approval statuses (Task 11), DS §, notes, mockup/render paths, spec §5–6.

- [ ] **Step 1: Write the 4 Phase-1 foundation items** using this skeleton for every item:

```markdown
### <working title>  (Paperclip: create as NEX-___)
**Phase:** 1-Foundation | 2-Page
**Blocked by:** <item titles or "—">
**Files:** <exact paths>
**Design refs:** DS §<n>; notes/<page>.md (approved <date>); renders/<page>_{dark,light}.png
**Change summary:** <3–6 imperative bullets>
**Acceptance criteria:**
- Visual match to the APPROVED render, dark + light
- Guardrails DS §9: no new timers/animation; shadow count ≤ <n>; QSS-first; FR-97 intact
- Cold-start and idle CPU unchanged within noise (macOS + Linux)
- CHANGELOG.md + docs/APPLICATION_OVERVIEW.md updated
**UAT (business-user steps):** <numbered tap/navigate/verify steps>
```

F1 shared card container (DS §2); F2 section-header pattern (DS §3); F3 state
patterns (DS §5); F4 QSS consolidation groundwork.

- [ ] **Step 2: Write one Phase-2 item per APPROVED page** (deferred pages get a
one-line "Deferred by maintainer <date>" entry, no item), each blocked by F1–F4,
ordered Processes, Resources, SystemCleaner first.

- [ ] **Step 3: Cross-check** — every note with status `Approved` has an item:
`grep -l "^**Status:** Approved" docs/design/ux-modernization/mockups/notes/*.md`
names must all appear in work-items.md; count of `### ` headings = 4 + approved-page count.

- [ ] **Step 4: Commit** — `git add docs/design/ux-modernization/work-items.md && git commit -m "docs(design): add Paperclip work-item drafts for approved prototypes"`

---

### Task 13: Package check + PR

**Files:**
- Modify: `docs/superpowers/specs/2026-07-13-ux-modernization-design.md` (tick §8 boxes)

- [ ] **Step 1: Run the spec §8 acceptance checklist** with evidence in hand:
capture coverage vs CAPTURE_NOTES.md; 5 spot-checked DS citations; render count
(`ls mockups/renders/*.png | wc -l` ≥ 38 = 18 pages × 2 + smoke); every note has
a status; work-items cross-check (Task 12 Step 3); and the no-code-change proof:
`git diff native --stat -- shared/ linux/ macos/ tests/` → **empty**.

- [ ] **Step 2: Push and open the PR**

```bash
git push
gh pr create --title "docs(design): UX modernization prototype package" \
  --body "$(cat <<'EOF'
## Summary
- Evidence pack: current-state captures of every page (macOS local + homelab Linux), zero code changes
- docs/design/DESIGN_SYSTEM.md: the 2.8.x dashboard language as a cited, normative spec
- 18 before/after page prototypes (token-true HTML → PNG, dark+light) with sourced rationale notes, reviewed and approved page-by-page by the maintainer
- Paperclip work-item drafts for the foundation components and every approved page

## Test plan
- [ ] No diffs under shared/, linux/, macos/, tests/
- [ ] Spec §8 acceptance checklist ticked with evidence

🤖 Generated with [Claude Code](https://claude.com/claude-code)
EOF
)"
```

Expected: PR URL printed — report it to Luke. **Do not merge.**

- [ ] **Step 3: Commit the ticked spec** — `git add docs/superpowers/specs/2026-07-13-ux-modernization-design.md && git commit -m "docs(design): tick spec acceptance criteria after package review" && git push`
