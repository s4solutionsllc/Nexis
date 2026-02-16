# BUG-37 Implementation Plan: Fix scanLoading.gif animation

## Overview

Restructure QMovie lifecycle so loading GIF animations are pre-initialised in the constructor, reused on theme change, and explicitly started/stopped at show/hide time.

---

## Task 1: Pre-initialise QMovie objects in the constructor

- [ ] In `system_cleaner_page.cpp` constructor: Create both `mLoadingMovie` and `mLoadingMovie_2` using the default theme name from `AppManager::ins()->resolveThemeName()`. Call `setMovie()` on both labels. Do NOT call `start()` yet (movies start when the label is shown).

## Task 2: Restructure the theme-change lambda to reuse existing QMovie objects

- [ ] In the `sigChangedAppTheme` lambda (lines 76–88): Instead of `new QMovie(...)`, call `mLoadingMovie->setFileName(newPath)` on the existing objects. Call `start()` only if the label is currently visible (i.e. a scan/clean is in progress). Remove the `hide()` calls (labels are already hidden by default; hiding mid-animation would be disruptive).

## Task 3: Add `start()` before `show()` in scan and clean handlers

- [ ] In `on_btnScan_clicked()` (line 388): Add `mLoadingMovie->start();` immediately before `ui->lblLoadingScanner->show();`.
- [ ] In `on_btnClean_clicked()`: Add `mLoadingMovie_2->start();` immediately before `ui->lblLoadingCleaner->show();`.

## Task 4: Add `stop()` when hiding the loading labels

- [ ] In `onScanFinished()`: Add `mLoadingMovie->stop();` when the scan results page is shown (label hidden implicitly by page change).
- [ ] In `onCleanFinished()`: Add `mLoadingMovie_2->stop();` when clean results are displayed.

## Task 5: Build verification and tracking

- [ ] Incremental build to verify compilation.
- [ ] Mark BUG-37 as `[x]` in `BUGS.md` with resolution note.
- [ ] Commit and push.
