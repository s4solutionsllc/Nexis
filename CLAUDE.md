# Nexis — Claude Code Project Instructions

## Project Overview

Nexis is a Linux & macOS System Optimizer and Monitoring tool (C++17, Qt6). Originally forked from [oguzhaninan/Stacer](https://github.com/oguzhaninan/Stacer), now rebranded and independently developed.

## Maintainer Operating Model (read before doing work here)

Nexis is a **reference product**, not a revenue line. Hard rules — see [`docs/MAINTAINER_SOP.md`](docs/MAINTAINER_SOP.md) for the full SOP and rationale:

- **Always free.** GPL-3.0-only, no monetization, ever. The release runbook ([`RELEASE.md`](RELEASE.md) §0) checks this on every release.
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

RELEASE_POLICY:
  track: train
  cadence: on-threshold:3-feats
  artifacts: [deb, appimage, dmg]
  hotfixFromBranch: true     # patch off the last tag; don't drag unreleased feats into a hotfix


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

> **Do not edit `CHANGELOG.md` directly, and do not hand-bump the generated
> stats table.** Both were shared single-line regions that every concurrent PR
> was required to touch, so the first PR to merge left every other open PR
> conflicting for no substantive reason (SSO-23951). The intent below is
> unchanged; only the mechanism moved.

**Release notes — add a fragment, not a `CHANGELOG.md` edit.**
Every user-visible change (feature, bug fix, or other notable change) adds one
new file `changelog.d/<slug>.<type>.md`, where `<type>` is one of `added`,
`changed`, `deprecated`, `removed`, `fixed`, `security`. The body is the entry
itself, no leading `- `. Distinct filenames never conflict. See
[`changelog.d/README.md`](changelog.d/README.md); validate with
`python3 scripts/changelog_fragments.py lint`. Fragments are folded into
`CHANGELOG.md` in Keep a Changelog format at release cut (`RELEASE.md` §1) —
which is still the file the website parses to show users release notes.

**Two living documents remain hand-maintained:**

- **`docs/APPLICATION_OVERVIEW.md`** — What the app does. Update the prose and
  the *curated components* table when features, UI elements, pages, or platform
  support change. The `NEXIS-STATS` block (version, LOC, source-file count,
  test-executable and test-method counts, translation count) is **generated** —
  leave it alone; `scripts/gen_doc_stats.py` owns it and the release cut runs it.
- **`docs/ARCHITECTURE_REVIEW.md`** — How the architecture works. Update **only
  when the architecture actually changes** — signals, singletons, timer/polling
  patterns, or cross-component communication. A change that adds no new
  cross-component wiring needs no edit here.

**Pre-commit checklist** (when a change affects documented behavior):
1. Add a `changelog.d/` fragment for anything user-visible.
2. Update the relevant feature/architecture section with the new or changed behavior.
3. Update the curated components table only if you added or removed a page,
   provider, service, manager, or tool class.
4. Do **not** update the "Last updated" date, the version header, or the
   generated stats — the release cut handles all three
   (`scripts/check_doc_versions.sh` enforces the version header at release time).

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

## AI-Vendor Support Ticket Content (SSO-17821, SSO-17824, SSO-17825)

Filing a support ticket, bug report, feedback submission, rating, or reproduction case with an AI vendor (Anthropic, Voyage AI/MongoDB, or any subprocessor) about Nexis or this repo is governed by the org-wide **AI-Vendor Submission Content Rule** — see the `standing-protocols` skill, section "AI-Vendor Submission Content Rule". It binds every agent and everyone acting on S4's behalf, including repo contributors. In brief:

- **No real user or client content** — including screenshots and free-text fields — in the ticket or payload. Reproduce with synthetic or structure-only redacted data.
- **No vendor-console feedback/rating controls** on any session that touched real data.
- **No engineer-level exception exists.** The exception ladder lives entirely in the standing protocol.
- **This is a failure-rate control, not an elimination control** — screenshots and free-text ticket fields are not covered by any tooling; check them by hand every time.
- **SSO-99 (external-action Board approval) applies separately** to the act of filing the ticket. Satisfying this content rule does not waive SSO-99, and SSO-99 approval does not waive this rule.
- **If it already happened:** do not delete or edit the ticket. File a child issue to CISO and GeneralCounsel in the same heartbeat with the ticket URL, vendor, and contents submitted.

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

- **QuentiumYT/Stacer** — Most active upstream fork (114 stars, v1.7.0 May 2026, now the Debian upstream for `stacer`). Reference for fixes and features.

## graphify

This project has a knowledge graph at graphify-out/ with god nodes, community structure, and cross-file relationships.

Rules:
- For codebase questions, first run `graphify query "<question>"` when graphify-out/graph.json exists. Use `graphify path "<A>" "<B>"` for relationships and `graphify explain "<concept>"` for focused concepts. These return a scoped subgraph, usually much smaller than GRAPH_REPORT.md or raw grep output.
- If graphify-out/wiki/index.md exists, use it for broad navigation instead of raw source browsing.
- Read graphify-out/GRAPH_REPORT.md only for broad architecture review or when query/path/explain do not surface enough context.
- After modifying code, run `graphify update .` to keep the graph current (AST-only, no API cost).
