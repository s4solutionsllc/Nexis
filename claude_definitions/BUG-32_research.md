# BUG-32 Research: GNOME Settings speed sliders spawn subprocess per pixel of drag

## Problem

In `gnome_mouse_tab.cpp`, the mouse and touchpad speed sliders connect `QSlider::valueChanged` directly to a lambda that calls `GnomeSettingsTool::setD()`. Since BUG-31, `setD()` calls `CommandUtil::execWithStatus()` which spawns a `gsettings set` (or `defaults write`) subprocess.

`QSlider::valueChanged` fires on **every discrete value change** during a drag. The sliders have range -100 to 100 (201 discrete values), so dragging from one end to the other spawns up to 200 subprocess calls. Each call involves:
1. `fork()` + `exec()` overhead
2. `waitForStarted()` + `waitForFinished()` blocking calls
3. `gsettings` connecting to dbus, writing, and disconnecting

This blocks the UI thread during each subprocess call (the code is synchronous), making the slider feel laggy and unresponsive during drags.

## Affected Code

### Slider connections (lines 35-46, 85-96 in `gnome_mouse_tab.cpp`)

**Mouse speed slider:**
```cpp
connect(ui->sliderMouseSpeed, &QSlider::valueChanged, this, [this](int val) {
    if (mLoading) return;
    double speed = val / 100.0;
    double prevSpeed = GnomeSettingsTool::getD(GnomeSchema::MOUSE, GnomeKey::SPEED);
    if (!GnomeSettingsTool::setD(GnomeSchema::MOUSE, GnomeKey::SPEED, speed)) {
        // revert...
    } else {
        ui->lblMouseSpeedVal->setText(QString::number(speed, 'f', 2));
    }
});
```

**Touchpad speed slider:**
```cpp
connect(ui->sliderTouchpadSpeed, &QSlider::valueChanged, this, [this](int val) {
    if (mLoading) return;
    double speed = val / 100.0;
    double prevSpeed = GnomeSettingsTool::getD(GnomeSchema::TOUCHPAD, GnomeKey::SPEED);
    if (!GnomeSettingsTool::setD(GnomeSchema::TOUCHPAD, GnomeKey::SPEED, speed)) {
        // revert...
    } else {
        ui->lblTouchpadSpeedVal->setText(QString::number(speed, 'f', 2));
    }
});
```

### UI configuration (gnome_mouse_tab.ui)

Both sliders are `QSlider` with:
- Range: -100 to 100
- Orientation: Horizontal
- No `tracking` property set (defaults to `true` — fires continuously during drag)

### Other valueChanged connections

There are also `QSpinBox::valueChanged` and `QDoubleSpinBox::valueChanged` connections in the Appearance tab (cursor size, text scaling) and WM tab (workspace count). These are less problematic since spin boxes fire one event per click/step, not continuously during drag. However, holding the up/down arrow button on a spin box will auto-repeat, so debouncing would still be a mild improvement.

**Scope decision:** Only debounce the two `QSlider` connections in `gnome_mouse_tab.cpp`. Spin boxes are not a practical concern.

## Solution Approach

Use a `QTimer` per slider (member variable) in restart/debounce mode:

1. When `valueChanged` fires, update the display label immediately (for responsive UI feedback) and restart a timer.
2. When the timer fires (after 200ms of no further value changes), execute the actual `gsettings set` call.
3. If the set call fails, revert the slider and label using `QSignalBlocker`.

This is a standard Qt debounce pattern. Two member `QTimer*` are needed (one per slider) since they may be dragged independently.

### Alternative considered: `QSlider::sliderReleased`

Could connect only to `sliderReleased` instead of `valueChanged`. But this misses:
- Arrow key changes (no release signal)
- Scroll wheel changes (no release signal)
- Programmatic changes

The timer approach handles all input methods correctly.

## Files to modify

1. `shared/nexis/Pages/GnomeSettings/gnome_mouse_tab.h` — Add two `QTimer*` members
2. `shared/nexis/Pages/GnomeSettings/gnome_mouse_tab.cpp` — Initialize timers, update slider lambdas to debounce
