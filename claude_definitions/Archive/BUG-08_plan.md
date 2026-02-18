# BUG-08 Plan: Wayland Compatibility

## Summary

The app crashes on Wayland because `primaryScreen()` returns `nullptr` before the compositor delivers screen info. Secondary issue: `raise()` and `activateWindow()` silently fail under Wayland's security model.

## Tasks

### Phase 1: Fix the crash — guard `primaryScreen()` null dereference

- [x] **1.1** `shared/nexis/app.cpp:32-35` — Wrap `qApp->primaryScreen()->availableGeometry()` in a null check. If `primaryScreen()` is null, skip `setGeometry()` (Qt will use the compositor's default window placement).

- [x] **1.2** `linux/nexis/Pages/StartupApps/startup_app_edit.cpp:33-36` — Same null-guard pattern.

- [x] **1.3** `macos/nexis/Pages/StartupApps/startup_app_edit.cpp:40-43` — Same null-guard pattern (defensive — macOS doesn't use Wayland, but prevents future breakage if the pattern is copied).

### Phase 2: Improve window activation for Wayland

- [x] **2.1** `shared/nexis/app.cpp:180-185` — In the tray icon activation lambda, replace `raise()` + `activateWindow()` with `windowHandle()->requestActivate()` (guarded by `windowHandle()` null check). This uses `xdg-activation` on Qt 6.3+ Wayland, which compositors may honor. Falls back gracefully on X11 where `requestActivate()` also works.

- [x] **2.2** `shared/nexis/app.cpp:206` — In `clickSidebarButton()`, replace `activateWindow()` with the same `windowHandle()->requestActivate()` pattern.

- [x] **2.3** Add `#include <QWindow>` to `shared/nexis/app.cpp` (needed for `windowHandle()->requestActivate()`).

### Phase 3: Build & verify

- [x] **3.1** Incremental build to confirm compilation.
- [x] **3.2** Update `BUGS.md` — mark BUG-08 as `[x]` with resolution note.

## Acceptance Criteria

- App compiles cleanly with no warnings from the changed files.
- `primaryScreen()` is never dereferenced without a null check.
- `raise()` and `activateWindow()` are no longer called for cross-window activation.
- No functional regression on X11 or macOS (both code paths are still valid).

## Files Changed

| File | Change |
|---|---|
| `shared/nexis/app.cpp` | Guard `primaryScreen()`, add `#include <QWindow>`, replace `raise()`/`activateWindow()` with `requestActivate()` |
| `linux/nexis/Pages/StartupApps/startup_app_edit.cpp` | Guard `primaryScreen()` |
| `macos/nexis/Pages/StartupApps/startup_app_edit.cpp` | Guard `primaryScreen()` |
| `BUGS.md` | Mark BUG-08 resolved |
