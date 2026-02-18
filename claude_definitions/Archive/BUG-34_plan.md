# BUG-34 Implementation Plan: Fix link color to Nexis orange

## Overview

Replace hardcoded blue `#007af4` with Nexis accent orange `#E95420` in the "Luke Simpson" GitHub profile link on the Settings page. Two locations.

---

## Task 1: Fix link color

- [x] In `settings_page.cpp`: replace `color:#007af4` with `color:#E95420` in the `lblCreatedBy` setText call.
- [x] In `settings_page.ui`: replace `color:#007af4` with `color:#E95420` in the `lblCreatedBy` default text.

## Task 2: Build verification and tracking

- [x] Incremental build to verify compilation.
- [x] Mark BUG-34 as `[x]` in `BUGS.md` with resolution note.
- [x] Commit and push.
