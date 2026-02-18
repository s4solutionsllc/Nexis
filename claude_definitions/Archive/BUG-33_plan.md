# BUG-33 Implementation Plan: Fix Purge checkbox text visibility in dark mode

## Overview

Add a `QCheckBox { color: @color05; }` rule to the QSS so all standard checkboxes use the theme's primary text color.

---

## Task 1: Add QCheckBox text color rule

- [x] In `shared/nexis/static/themes/default/style/style.qss`: Insert `QCheckBox { color: @color05; }` between the section header comment (line 139) and the `::indicator` rule (line 141).

## Task 2: Build verification and tracking

- [x] Incremental build to verify compilation.
- [x] Mark BUG-33 as `[x]` in `BUGS.md` with resolution note.
- [x] Commit and push.
