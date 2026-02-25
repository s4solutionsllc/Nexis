# Nexis — Claude Code Project Instructions

## Project Overview

Nexis is a Linux & macOS System Optimizer and Monitoring tool (C++/Qt). Originally forked from [oguzhaninan/Stacer](https://github.com/oguzhaninan/Stacer), now rebranded and independently developed.

## Tracking Files

Two tracking files live at the project root. Claude Code must keep them up to date as work progresses:

- **`FEATURE_REQUESTS.md`** — Feature request backlog with `FR-XX` IDs.
- **`BUGS.md`** — Bug backlog with `BUG-XX` IDs, sorted by severity (HIGH > MEDIUM > LOW).

### Conventions

- Each item has a checkbox status: `[ ]` open/planned, `[~]` in progress, `[x]` done.
- When starting work on an item, change its status to `[~]`.
- When finishing work on an item, change its status to `[x]` and add a line: `**Resolved:** <commit hash or brief note>`.
- New items go at the end of their severity section (bugs) or category section (features), using the next sequential ID.
- Never remove items — mark them `[x]` when done so history is preserved.
- If a bug or feature is discovered during a session, add it to the appropriate file immediately.

### Referencing Items

When committing code that addresses a tracked item, include the ID in the commit message. Example:
```
Fix swapped memory variables (BUG-01)
```

## Build

**EXECUTE WITHOUT ASKING:** Run `cmake` and `make` commands automatically when the user asks for a build or rebuild. Do not prompt for confirmation.

```bash
# Clean rebuild (from project root)
rm -rf build && cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=$(brew --prefix qt@6) && cmake --build build -j$(sysctl -n hw.ncpu)

# Incremental rebuild
cmake --build build -j$(sysctl -n hw.ncpu)
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

- `nexis-core/` — Core library (system info, utilities)
- `nexis/` — Qt GUI application (pages, widgets)
- `shared/` — Shared code between platforms
- `linux/` — Linux-specific implementations
- `translations/` — i18n `.ts` files
- `tests/` — Qt Test unit tests (CTest integration)

## Feature / Bug Resolution Workflow

When the user requests a new feature or asks to fix a bug, follow this three-phase workflow **automatically**. All artifacts go in the `backlog/` folder.

### Phase 1 — Research (`{ID}_research.md`)

1. Create `backlog/{ID}_research.md` (e.g., `FR-25_research.md` or `BUG-31_research.md`).
2. Perform deep research on the request:
   - Read all relevant source files, headers, `.ui` files, QSS, and CMakeLists.txt.
   - Trace call chains, signal/slot connections, and data flow end-to-end.
   - Understand how the current behavior works, what it does, and all its specificities.
   - Identify edge cases, platform differences (macOS vs Linux), and potential side effects.
   - Review upstream Stacer and QuentiumYT forks for prior art or related fixes.
3. Write a detailed report of all findings in the research file. Include file paths, line numbers, code snippets, and architectural context.

### Phase 2 — Plan (`{ID}_plan.md`)

1. Create `backlog/{ID}_plan.md`.
2. Write a detailed implementation plan with:
   - Numbered tasks/phases, each with specific files and changes.
   - Checkboxes (`[ ]`) for each task to track completion.
   - Clear acceptance criteria for each task.
   - Build verification steps.
3. **Wait for user approval before proceeding to Phase 3.**

### Phase 3 — Implementation

1. Once the user approves the plan, implement it fully. Do not stop until all tasks and phases are completed.
2. As each task/phase is completed, mark it `[x]` in the plan document.
3. Run incremental builds after each significant change to catch issues early.
4. **Code quality rules during implementation:**
   - Do not add unnecessary comments.
   - Do not use `any` or unknown types.
   - Continuously verify you are not introducing new issues (build checks, grep for regressions).
5. Update `BUGS.md` or `FEATURE_REQUESTS.md` with resolution notes and commit hash.
6. Update project documentation (see **Documentation Maintenance** below).
7. Commit and push when complete.

### Archiving Completed Work

When a bug or feature request is marked `[x]` (done), move its associated `backlog/` files (`{ID}_research.md`, `{ID}_plan.md`, and any other `{ID}_*.md` variants) to `backlog/Archive/`. This keeps the active working directory clean and limited to open/in-progress items. Files for open (`[ ]`) or in-progress (`[~]`) items must remain in `backlog/`.

## Documentation Maintenance

Two living documents in `docs/` must be kept in sync with the codebase as features are added or bugs are fixed:

- **`docs/APPLICATION_OVERVIEW.md`** — What the app does and how it's built. Update when:
  - A new feature is implemented (add to the relevant page section, update counts/stats)
  - A page gains new UI elements, gauges, buttons, or modes
  - Architecture changes (new managers, signals, build targets, themes, etc.)
  - Platform support changes (new backends, new conditional pages)

- **`docs/ARCHITECTURE_REVIEW.md`** — How the architecture works, strengths, weaknesses, and recommendations. Update when:
  - New architectural patterns are introduced (new signals, new singletons, new cross-component communication)
  - Existing weaknesses are addressed or new ones are discovered
  - Signal counts change on SignalMapper (the review tracks this)
  - Timer/polling patterns change
  - The recommended improvements list needs revision

**When to update:** After completing any feature (`[x]` in FEATURE_REQUESTS.md) or bug fix (`[x]` in BUGS.md), review both documents and make targeted updates to reflect the changes. Keep updates concise — modify existing sections rather than appending paragraphs.

## Qt/QSS Gotchas

### QScrollArea Viewport in Programmatic Dialogs

When creating a `QScrollArea` inside a programmatically-built `QDialog`, the viewport renders with the system palette (white) instead of the dialog's QSS-themed background. `QPalette` propagation, `setAutoFillBackground(false)`, and `WA_TranslucentBackground` do **not** work due to timing/precedence conflicts between Qt's palette system and QSS.

**Working pattern** (matches HardwareInfoPage):
```cpp
scrollArea->setFrameShape(QFrame::NoFrame);
scrollArea->setStyleSheet("QScrollArea{background-color:transparent;}");
scrollWidget->setStyleSheet("background-color:transparent;");
```
This makes the scroll area and its content transparent so the dialog's QSS `background-color: @color01` shows through.

## Custom Commands

Five project-specific slash commands are available in `.claude/commands/`:

| Command | Purpose | When to Use |
|---------|---------|-------------|
| `/session-start` | Initialize a work session with project status summary | Start of every new session |
| `/build-verify` | Incremental or clean build + test cycle | After any code change; accepts `clean` or `quick` args |
| `/bug-feature-workflow` | Three-phase Research → Plan → Implementation | When starting work on any BUG-XX or FR-XX item |
| `/qt-ui-change` | Qt/QSS verification checklist | After any UI modification (pages, widgets, themes, QSS) |
| `/platform-check` | Cross-platform compatibility audit | After modifying shared code that touches platform APIs |

## Qt/QSS Additional Gotchas

### Hardcoded Colors (BUG-47)
Never use hardcoded hex colors in C++ code. All colors must come from `values.ini` theme tokens resolved at runtime via `AppManager::getStyleValues()`. Widgets should store token name strings (e.g., `"@cpuColor"`) and implement a `refreshThemeColors()` method connected to `SignalMapper::sigChangedAppTheme`.

### Token Name Collisions (BUG-49)
Token names must not be substrings of other token names. `AppManager::updateStylesheet()` sorts keys by descending length before replacement, but avoid creating tokens like `@card` and `@cardBg` where the shorter is a prefix of the longer.

### QPushButton vs QToolButton on macOS (BUG-52)
macOS Qt6's `QPushButton` fails to render SVG icons for icon-only buttons with `background: transparent`. Use `QToolButton` with `setAutoRaise(true)` instead.

### Dynamic Property Re-polish (BUG-56)
After changing a QSS dynamic property on a parent widget, child widgets must be explicitly re-polished with `unpolish()`/`polish()` — Qt does not recursively re-evaluate property-dependent selectors.

## Notable Forks

- **QuentiumYT/Stacer** — Most active fork of the original project (78 stars). Reference for fixes and features.
