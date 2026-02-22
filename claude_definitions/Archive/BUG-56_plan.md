# BUG-56 Plan: Fix navbar items centered after expanding from collapsed state

## Root Cause

`applySidebarCollapse()` sets the `collapsed` dynamic property on `#sidebar` and calls `unpolish()`/`polish()` on the sidebar only. Qt does not recursively re-polish child widgets, so the child `QPushButton` items retain the stale `text-align: center` from the `#sidebar[collapsed="true"] QPushButton` QSS rule.

## Fix

### Task 1: Re-polish child buttons after sidebar property change

- [x] **File:** `shared/nexis/app.cpp`, `applySidebarCollapse()` (lines 708-710)
- **Change:** After the existing `unpolish`/`polish` on `ui->sidebar`, iterate over all child `QPushButton` widgets in `mListSidebarButtons` and call `unpolish()`/`polish()` on each one. Also re-polish `btnFeedback` and `mBtnSidebarToggle` since they are also targeted by the collapsed QSS selectors.
- **Acceptance criteria:** After expanding a collapsed sidebar (whether from startup restore or manual collapse), all nav items display left-aligned text matching the `#sidebar QPushButton { text-align: left }` rule.

### Task 2: Build verification

- [x] Run incremental build to confirm no compilation errors.

## Notes

- This is a minimal, targeted fix — no QSS changes needed.
- The same pattern (re-polishing children after parent property change) is a standard Qt practice for dynamic property-dependent child selectors.
- No risk of side effects — `unpolish`/`polish` simply forces Qt to re-read the stylesheet for each widget.
