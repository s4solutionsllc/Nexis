# Nexis — Claude Code Project Instructions

## Project Overview

Nexis is a Linux & macOS System Optimizer and Monitoring tool (C++17, Qt6). Originally forked from [oguzhaninan/Stacer](https://github.com/oguzhaninan/Stacer), now rebranded and independently developed.

## Maintainer Operating Model (read before doing work here)

Nexis is a **reference product**, not a revenue line. Hard rules — see [`docs/MAINTAINER_SOP.md`](docs/MAINTAINER_SOP.md) for the full SOP and rationale:

- **Always free.** GPL-3.0-or-later, no monetization, ever. The release runbook ([`RELEASE.md`](RELEASE.md) §0) checks this on every release.
- **Staffing:** NexisMaintainer is full-time on Nexis. Plan work in normal product-development terms; do not treat engineering hours as a constrained budget.
- **Maintainer of record:** EngineeringLead. **Escalation owner:** CEO (product-strategy, monetization, platform-expansion decisions, sponsorships, anything in `RELEASE.md` §0).
- **Cadence:** continuous development, with release windows roughly every 4–6 weeks for user-visible features. CVE/security and critical bugs (data loss, app refuses to launch on a fresh install of a supported platform, system-level harm caused by a Nexis action) are still treated as interrupts and patched out within the SLA in `RELEASE.md`.
- **Community SLAs:** triage acknowledgment on new GitHub issues within 7 days; review of community PRs within 7 days.
- **Releases:** see [`RELEASE.md`](RELEASE.md). Tag-driven (`vX.Y.Z`). Do not edit GitHub Release prose by hand — edit `CHANGELOG.md` and re-tag.
- **CVE SLA:** patch out within **7 calendar days** of credible disclosure (`RELEASE.md` §6).

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

## Documentation Maintenance

Three living documents must be kept in sync **before committing** any completed work:

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

## Work Item Tracking

**Paperclip is the system of record** for all issues, features, and tracking. The issue-ID prefix is `NEX` — reference issues as `NEX-<number>` in commit messages and PRs.

There is **no Paperclip MCP server** — it is human-driven. Reference issues by their `NEX-<number>` ID; when you need an issue's scope or details, ask the user rather than looking it up programmatically.

**The old self-hosted Plane instance has been decommissioned.** Do **not** call any `mcp__plane__*` tools — the `plane` MCP server is gone and those calls will fail.

## GitHub Issues Sync (Run at Every Session Start)

**EXECUTE WITHOUT ASKING.** At the start of every session, fetch open GitHub issues and report them. No automated sync to Paperclip (its MCP is not yet connected).

```bash
gh issue list --repo s4solutionsllc/Nexis --state open --limit 100 --json number,title,body,labels
```

Report open issues grouped by label (bugs vs enhancements). Note any that look actionable for the current session.

## Feature / Bug Resolution Workflow (Project Override)

This extends the global Phase 3 workflow.

### Phase 3 — Implementation

1. Implement fully once approved. Do not stop until all tasks are completed.
2. Mark each task `[x]` in the plan as completed.
3. Reference the GitHub issue number in commit messages (`GH#NN`).

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
