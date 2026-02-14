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

```bash
# From project root
mkdir -p build && cd build
cmake ..
make -j$(nproc)
```

## Key Directories

- `nexis-core/` — Core library (system info, utilities)
- `nexis/` — Qt GUI application (pages, widgets)
- `shared/` — Shared code between platforms
- `linux/` — Linux-specific implementations
- `translations/` — i18n `.ts` files

## Notable Forks

- **QuentiumYT/Stacer** — Most active fork of the original project (78 stars). Reference for fixes and features.
