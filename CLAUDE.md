# Nexis — Claude Code Project Instructions

## Project Overview

Nexis is a Linux & macOS System Optimizer and Monitoring tool (C++17, Qt6). Originally forked from [oguzhaninan/Stacer](https://github.com/oguzhaninan/Stacer), now rebranded and independently developed.

## Maintenance Time-Box (read before doing work here)

Nexis is a **reference product**, not a revenue line. Hard rules — see [`docs/MAINTAINER_SOP.md`](docs/MAINTAINER_SOP.md) for the full SOP and rationale:

- **Always free.** GPL-3.0-or-later, no monetization, ever. The release runbook ([`RELEASE.md`](RELEASE.md) §0) checks this on every release.
- **Maintainer of record:** EngineeringLead. **Escalation owner:** CEO (security/legal only).
- **Time-box:** ≤10% of EngineeringLead capacity per quarter. Hard ceiling 15% **with prior CEO approval only.** Surface a projected overage in advance, not after the fact.
- **On-call cadence:** only **CVE / security reports** and **critical bugs** (data loss, app refuses to launch on a fresh install of a supported platform, system-level harm caused by a Nexis action) trigger an out-of-cadence wake. Everything else batches into the **quarterly maintenance pass**.
- **Releases:** see [`RELEASE.md`](RELEASE.md). Tag-driven (`vX.Y.Z`). Do not edit GitHub Release prose by hand — edit `CHANGELOG.md` and re-tag.
- **CVE SLA:** patch out within **7 calendar days** of credible disclosure (`RELEASE.md` §6).

If a request would push past the time-box, refuse politely and surface it to the CEO with a budget impact summary; do not silently grow the surface area.

## Tracking Files

- **`FEATURE_REQUESTS.md`** — Feature request backlog with `FR-XX` IDs, organized by category.
- **`BUGS.md`** — Bug backlog with `BUG-XX` IDs, sorted by severity (HIGH > MEDIUM > LOW).

Conventions for these files are defined in the global CLAUDE.md.

## SQLite Index

`backlog/nexis.db` is a derived SQLite index of the structured fields in `FEATURE_REQUESTS.md` and `BUGS.md`. Use `scripts/nexis_db.py` for all queries and status updates — never write to the DB with raw SQL outside of that script.

The markdown files remain authoritative for full content. If the DB drifts out of sync (e.g., after a manual markdown edit), run:
```bash
python scripts/nexis_db.py sync
```
This re-parses both files and reconciles the DB. It is safe to run at any time.

Common queries:
```bash
python scripts/nexis_db.py summary          # session-start counts
python scripts/nexis_db.py open             # all open items
python scripts/nexis_db.py open --type bug  # open bugs only
python scripts/nexis_db.py in-progress      # active work
python scripts/nexis_db.py tracked --issue 42  # check if GH issue is tracked
```

## Build

**EXECUTE WITHOUT ASKING:** Run `cmake` and `make` commands automatically. Do not prompt for confirmation.

```bash
# Clean rebuild — macOS (from project root)
rm -rf build && cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=$(brew --prefix qt@6) && cmake --build build -j$(sysctl -n hw.ncpu)

# Clean rebuild — Linux
rm -rf build && cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j$(nproc)

# Incremental rebuild (either platform)
cmake --build build -j$(nproc 2>/dev/null || sysctl -n hw.ncpu)
```

## Testing

Framework: Qt Test (QTest) with CTest integration. Tests build by default (`BUILD_TESTING=ON`).

```bash
# Run tests (after building)
ctest --test-dir build --output-on-failure

# Build without tests
cmake -B build -DBUILD_TESTING=OFF ...
```

**Adding a new test:**
1. Create `tests/<category>/test_<classname>.cpp` (categories: `utils/`, `core/`, `managers/`)
2. Use `QTEST_MAIN(TestClassName)` and `#include "test_<classname>.moc"` at the end
3. Add the file to the source list in `tests/CMakeLists.txt`
4. Register with CTest via `add_test(NAME <TestName> COMMAND nexis-tests)`

## Key Directories

The codebase splits shared and platform-specific code:

- `shared/nexis-core/` — Core library: system info, utilities (cross-platform)
- `shared/nexis/` — Qt GUI app: Pages, Managers, Services, SignalMapper (cross-platform)
- `macos/nexis-core/` — macOS-specific core implementations (Obj-C++ bridges)
- `macos/nexis/` — macOS-specific GUI code
- `linux/nexis-core/` — Linux-specific core implementations
- `linux/nexis/` — Linux-specific GUI code
- `tests/` — Qt Test unit tests (categories: `utils/`, `core/`, `managers/`, `theme/`)
- `translations/` — i18n `.ts` files
- `scripts/` — Build and utility scripts
- `docs/` — Living documentation (overview, architecture review, roadmap)
- `backlog/` — Research and plan artifacts for in-progress work

## Documentation Maintenance

Three living documents must be kept in sync **before committing** any `[x]` item:

- **`CHANGELOG.md`** — User-facing release notes. **Must be updated with every version bump.** Use [Keep a Changelog](https://keepachangelog.com/en/1.1.0/) format with `## [version] - date` headers and `### Added` / `### Fixed` / `### Changed` subsections. Every feature, bug fix, or notable change included in a release must have an entry here. This file is parsed by the website to display release notes to users.
- **`docs/APPLICATION_OVERVIEW.md`** — What the app does. Update when features, UI elements, architecture, or platform support changes.
- **`docs/ARCHITECTURE_REVIEW.md`** — How the architecture works. Update when signals, singletons, timer/polling patterns, or cross-component communication changes.

**Pre-commit checklist** (when a change affects documented behavior):
1. Update `CHANGELOG.md` under the current version's section (create one if it doesn't exist).
2. Update the "Last updated" date and version in both docs if stale.
3. Update any affected stats (page count, signal count, test count, feature/bug counts, line counts, etc.).
4. Update the relevant feature/architecture section with the new or changed behavior.

Keep updates concise — modify existing sections rather than appending paragraphs.

## Qt/QSS Gotchas

### QScrollArea Viewport in Programmatic Dialogs
`QScrollArea` inside a programmatic `QDialog` renders with system palette instead of QSS theme. Fix:
```cpp
scrollArea->setFrameShape(QFrame::NoFrame);
scrollArea->setStyleSheet("QScrollArea{background-color:transparent;}");
scrollWidget->setStyleSheet("background-color:transparent;");
```

### Hardcoded Colors (BUG-47)
Never use hardcoded hex colors in C++. All colors come from `values.ini` theme tokens via `AppManager::getStyleValues()`. Widgets store token strings (e.g., `"@cpuColor"`) and implement `refreshThemeColors()` connected to `SignalMapper::sigChangedAppTheme`.

### Token Name Collisions (BUG-49)
Token names must not be substrings of other tokens. `AppManager::updateStylesheet()` sorts by descending length, but avoid pairs like `@card` / `@cardBg`.

### QPushButton vs QToolButton on macOS (BUG-52)
macOS Qt6 `QPushButton` fails SVG icon rendering for icon-only transparent buttons. Use `QToolButton` with `setAutoRaise(true)`.

### Dynamic Property Re-polish (BUG-56)
After changing a QSS dynamic property on a parent, child widgets need explicit `unpolish()`/`polish()` — Qt doesn't recursively re-evaluate property selectors.

## GitHub Issues Sync (Run at Every Session Start)

**EXECUTE WITHOUT ASKING.** At the start of every session, sync GitHub issues to the local tracking files.

### Steps

1. **Fetch open issues:**
   ```bash
   gh issue list --repo s4solutionsllc/Nexis --state open --limit 100 --json number,title,body,labels
   ```

2. **Identify untracked issues** — For each open issue, run:
   ```bash
   python scripts/nexis_db.py tracked --issue <number>
   ```
   If the output is empty, the issue is untracked and needs to be added.

3. **Classify each untracked issue:**
   - **Bug** → add to `BUGS.md` if the issue title/labels contain: bug, fix, crash, error, broken, regression, incorrect, fail
   - **Feature Request** → add to `FEATURE_REQUESTS.md` otherwise (enhancement, feature, improvement, request, add, support, etc.)
   - When ambiguous, prefer Feature Request

4. **Add to the appropriate tracking file and index:**
   - Use the next sequential ID (`BUG-XX` or `FR-XX`)
   - Append to the markdown file: `- [ ] **BUG-XX / #<issue>**: <issue title>` under the correct section
   - Then add to the DB index:
     ```bash
     # For a bug:
     python scripts/nexis_db.py add --id BUG-XX --type bug --title "<title>" \
       --severity <high|medium|low> --issue <number> --category "<section>"
     # For a feature:
     python scripts/nexis_db.py add --id FR-XX --type feature --title "<title>" \
       --issue <number> --category "Uncategorized"
     ```

5. **Report** — After syncing, state how many new issues were added and list them.

If there are no untracked issues, state "GitHub issues up to date" and continue.

## Feature / Bug Resolution Workflow (Project Override)

This extends the global Phase 3 workflow with SQLite index updates.

### Phase 3 — Implementation

1. Implement fully once approved. Do not stop until all tasks are completed.
2. Mark each task `[x]` in the plan as completed.
3. When starting work on a tracked item, update the DB:
   ```bash
   python scripts/nexis_db.py start --id <ID>
   ```
   Also change `[ ]` → `[~]` in the markdown tracking file.
4. When closing a tracked item, update the DB:
   ```bash
   python scripts/nexis_db.py close --id <ID> --resolution "<brief note>" --commit <hash>
   # Or for declined items:
   python scripts/nexis_db.py close --id <ID> --declined --resolution "Will not implement — reason"
   ```
   Also change `[~]` → `[x]` in the markdown tracking file and add the `**Resolved:**` note.

## Custom Commands

| Command | Purpose | When to Use |
|---------|---------|-------------|
| `/session-start` | Project status summary + GitHub issue sync | Start of every session |
| `/build-verify` | Build + test cycle | After code changes; args: `clean`, `quick` |
| `/bug-feature-workflow` | Research → Plan → Implement | Starting any BUG-XX or FR-XX |
| `/qt-ui-change` | Qt/QSS verification checklist | After UI modifications |
| `/platform-check` | Cross-platform audit | After modifying platform-touching code |

## Notable Forks

- **QuentiumYT/Stacer** — Most active upstream fork (78 stars). Reference for fixes and features.
