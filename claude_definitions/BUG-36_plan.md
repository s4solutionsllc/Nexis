# BUG-36 Implementation Plan: Fix System Cleaner "Total Size" label visibility

## Overview

Add a QSS rule for `#lblTotalBytes` so it uses the theme's primary text color in dark mode.

---

## Task 1: Add QSS rule for lblTotalBytes

- [x] In `shared/nexis/static/themes/default/style/style.qss`: Add `#lblTotalBytes { font-size: 11pt; color: @color05; }` after the `#lblRemovedTotalSize` block (line 649).

## Task 2: Build verification and tracking

- [x] Incremental build to verify.
- [x] Mark BUG-36 as `[x]` in `BUGS.md` with resolution note.
- [x] Commit and push.
