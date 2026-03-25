# BUG-103 Plan: Fix schedule indicator position on initial page load

## Task 1: Add showEvent() override
- [x] Declare `void showEvent(QShowEvent *event) override;` in `system_cleaner_page.h`
- [x] Implement in `system_cleaner_page.cpp`: call base `QWidget::showEvent(event)`, then `repositionScheduleIndicator()` and `repositionExclusionsButton()`

## Task 2: Build and verify
- [x] Incremental build — clean, 0 warnings
- [x] All 26 tests pass

## Acceptance Criteria
1. Schedule indicator appears at the bottom of the page on first navigation (no white square at top-left)
2. Resize behavior unchanged
3. Exclusions button also correctly positioned on first show
