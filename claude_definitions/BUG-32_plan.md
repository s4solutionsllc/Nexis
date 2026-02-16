# BUG-32 Implementation Plan: Debounce speed slider writes

## Overview

Add timer-based debouncing to the mouse and touchpad speed sliders in `GnomeMouseTab` so that `gsettings set` / `defaults write` is only called once after the user stops dragging, instead of on every pixel of movement.

---

## Task 1: Add QTimer members to GnomeMouseTab

- [x] In `shared/nexis/Pages/GnomeSettings/gnome_mouse_tab.h`:
  - Add `#include <QTimer>` (or forward-declare)
  - Add two member variables: `QTimer *mMouseSpeedTimer;` and `QTimer *mTouchpadSpeedTimer;`

## Task 2: Initialize timers and refactor slider lambdas

- [x] In `shared/nexis/Pages/GnomeSettings/gnome_mouse_tab.cpp`:
  - In the constructor, create both timers with `setSingleShot(true)` and 200ms interval.
  - **Mouse speed slider lambda** (`sliderMouseSpeed valueChanged`):
    - Update `lblMouseSpeedVal` text immediately (visual feedback is instant).
    - Call `mMouseSpeedTimer->start(200)` to restart the debounce timer.
    - Remove the direct `GnomeSettingsTool::setD()` call from this lambda.
  - **Connect `mMouseSpeedTimer::timeout`** to a new lambda that:
    - Reads the current slider value, converts to speed.
    - Reads the previous system value via `getD()`.
    - Calls `setD()` and on failure, reverts slider + label with `QSignalBlocker` and emits `settingFailed`.
  - Same pattern for **touchpad speed slider** using `mTouchpadSpeedTimer`.

## Task 3: Build verification and tracking

- [x] Incremental build to verify compilation.
- [x] Mark BUG-32 as `[x]` in `BUGS.md` with resolution note.
- [x] Commit and push.
