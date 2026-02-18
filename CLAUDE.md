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

## Key Directories

- `nexis-core/` — Core library (system info, utilities)
- `nexis/` — Qt GUI application (pages, widgets)
- `shared/` — Shared code between platforms
- `linux/` — Linux-specific implementations
- `translations/` — i18n `.ts` files

## Feature / Bug Resolution Workflow

When the user requests a new feature or asks to fix a bug, follow this three-phase workflow **automatically**. All artifacts go in the `claude_definitions/` folder.

### Phase 1 — Research (`{ID}_research.md`)

1. Create `claude_definitions/{ID}_research.md` (e.g., `FR-25_research.md` or `BUG-31_research.md`).
2. Perform deep research on the request:
   - Read all relevant source files, headers, `.ui` files, QSS, and CMakeLists.txt.
   - Trace call chains, signal/slot connections, and data flow end-to-end.
   - Understand how the current behavior works, what it does, and all its specificities.
   - Identify edge cases, platform differences (macOS vs Linux), and potential side effects.
   - Review upstream Stacer and QuentiumYT forks for prior art or related fixes.
3. Write a detailed report of all findings in the research file. Include file paths, line numbers, code snippets, and architectural context.

### Phase 2 — Plan (`{ID}_plan.md`)

1. Create `claude_definitions/{ID}_plan.md`.
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
6. Commit and push when complete.

### Archiving Completed Work

When a bug or feature request is marked `[x]` (done), move its associated `claude_definitions/` files (`{ID}_research.md`, `{ID}_plan.md`, and any other `{ID}_*.md` variants) to `claude_definitions/Archive/`. This keeps the active working directory clean and limited to open/in-progress items. Files for open (`[ ]`) or in-progress (`[~]`) items must remain in `claude_definitions/`.

## Notable Forks

- **QuentiumYT/Stacer** — Most active fork of the original project (78 stars). Reference for fixes and features.
