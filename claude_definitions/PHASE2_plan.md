# Phase 2: Build System Hardening — Plan

**Tracking:** Architecture Review §1A
**Goal:** Replace all `GLOB_RECURSE` with explicit source lists in `CMakeLists.txt`.
**Approach:** Inline `set()` blocks (Option A from research).

---

## Task 2.1 — Replace CORE source globs with explicit `set()` lists

**File:** `CMakeLists.txt` (lines 37-40)

With explicit `set()` blocks for:
- `CORE_SHARED_SRCS` — 14 .cpp files
- `CORE_SHARED_HDRS` — 20 .h files
- `CORE_PLAT_SRCS` — platform-conditional block using `if(APPLE)` with 15 macOS .cpp files / 15 Linux .cpp files
- `CORE_PLAT_HDRS` — platform-conditional block with 3 macOS .h files / 2 Linux .h files

**Acceptance:**
- [x] `cmake -B build` succeeds
- [x] Incremental build compiles all core sources

---

## Task 2.2 — Replace GUI source globs with explicit `set()` lists

**File:** `CMakeLists.txt` (lines 77-80)

With explicit `set()` blocks for:
- `GUI_SHARED_SRCS` — 39 .cpp files
- `GUI_SHARED_HDRS` — 43 .h files
- `GUI_PLAT_SRCS` — platform-conditional block with 2 macOS .cpp / 2 Linux .cpp
- `GUI_PLAT_HDRS` — empty set (no platform GUI headers exist; kept as empty set for future use)

**Acceptance:**
- [x] `cmake -B build` succeeds
- [x] Incremental build compiles all GUI sources

---

## Task 2.3 — Replace translations glob with explicit `set()` list

**File:** `CMakeLists.txt` (line 83)

Replaced with explicit `set()` of 34 `.ts` files.

**Acceptance:**
- [x] `cmake -B build` succeeds
- [x] Translation files are still picked up by `qt_create_translation()`

---

## Task 2.4 — Add documentation comment in CMakeLists.txt

Added comment blocks at each source list section.

**Acceptance:**
- [x] Comment is present and clearly visible

---

## Task 2.5 — Full clean rebuild verification

**Acceptance:**
- [x] Clean build succeeds with zero errors
- [x] Application launches and runs normally

---

## Task 2.6 — Update tracking and documentation

- [x] Mark Phase 2 tasks as `[x]` in `docs/IMPLEMENTATION_ROADMAP.md`
- [x] Update `docs/ARCHITECTURE_REVIEW.md` §4 (CMake GLOB_RECURSE) to note it's resolved
- [x] Update CLAUDE.md build instructions if any changes are needed (no changes needed)
- [x] Commit with message: `chore(build): replace GLOB_RECURSE with explicit source lists (Phase 2)`

---

## Summary

| Task | Description | Est. Time | Status |
|------|-------------|-----------|--------|
| 2.1 | Replace core source globs | 15 min | Done |
| 2.2 | Replace GUI source globs | 20 min | Done |
| 2.3 | Replace translations glob | 5 min | Done |
| 2.4 | Add documentation comment | 2 min | Done |
| 2.5 | Full clean rebuild verification | 5 min | Done |
| 2.6 | Update tracking and docs | 10 min | Done |
| **Total** | | **~1 hour** | **Complete** |
