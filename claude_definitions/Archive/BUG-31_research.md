# BUG-31 Research: GNOME Settings silently fails when `gsettings set` errors

## Executive Summary

When `gsettings set` (Linux) or `defaults write` (macOS) fails, the error is silently swallowed. The `GnomeSettingsTool::set*()` methods catch the exception, log it with `qCritical()`, and return `void`. The calling lambda in each tab has no way to know the write failed, so the UI widget retains the user's new value even though the underlying setting was never applied. The user sees the change as successful when it actually did nothing.

There are **two independent bugs** compounding into this single symptom:

1. **`CommandUtil::exec()` does not detect non-zero exit codes** -- it only throws on `QProcess::error()` (process launch failures like "command not found"), not on the subprocess returning a non-zero exit code. So when `gsettings set` fails with an error message on stderr and exit code 1, `CommandUtil::exec()` returns successfully with empty stdout.

2. **`GnomeSettingsTool::set*()` returns `void`** -- even if an exception were thrown, the calling lambdas in all four tabs have no mechanism to detect failure and revert the widget.

---

## Layer-by-Layer Analysis

### Layer 1: `CommandUtil::exec()` (shared/nexis-core/Utils/command_util_shared.cpp)

```cpp
QString CommandUtil::exec(const QString &cmd, QStringList args, QByteArray data, int timeoutMs)
{
    std::unique_ptr<QProcess> process(new QProcess());
    process->start(cmd, args);

    if (! data.isEmpty()) {
        process->write(data);
        process->waitForBytesWritten();
        process->closeWriteChannel();
    }

    process->waitForFinished(timeoutMs);

    QTextStream stdOut(process->readAllStandardOutput());

    QString err = process->errorString();

    process->kill();
    process->close();

    if (process->error() != QProcess::UnknownError)
        throw err;

    return stdOut.readAll().trimmed();
}
```

**Critical flaw:** `QProcess::error()` returns `QProcess::UnknownError` when the process launched and ran successfully, *regardless of the process's exit code*. The Qt documentation states: "`QProcess::error()` returns the type of error that occurred last" and `UnknownError` means "No error has occurred." It does NOT reflect the subprocess exit code.

When `gsettings set org.gnome.desktop.interface gtk-theme "NonExistentTheme"` runs:
- The `gsettings` binary *launches* successfully (no `QProcess::FailedToStart`).
- The `gsettings` binary writes an error to stderr (e.g., `No such schema 'org.gnome.desktop.foo'` or `value "xyz" is not valid`).
- The `gsettings` binary exits with code 1.
- `QProcess::error()` returns `QProcess::UnknownError` (no *process* error occurred).
- `CommandUtil::exec()` does **NOT throw** -- it returns the empty stdout string.

**Missing:** The code never calls `process->exitCode()` or `process->readAllStandardError()`. The subprocess exit code is completely ignored. Stderr output is never read.

### Layer 2: `GnomeSettingsTool::set*()` (linux/nexis-core/Tools/gnome_settings_tool.cpp)

```cpp
void GnomeSettingsTool::setS(const QString &schema, const QString &key, const QString &value)
{
    try {
        CommandUtil::exec("gsettings", {"set", schema, key, value});
    } catch (const QString &ex) {
        qCritical() << "GnomeSettingsTool::setS failed:" << schema << key << value << ex;
    }
}

void GnomeSettingsTool::setB(const QString &schema, const QString &key, bool value)
{
    setS(schema, key, value ? "true" : "false");
}

void GnomeSettingsTool::setI(const QString &schema, const QString &key, int value)
{
    setS(schema, key, QString::number(value));
}

void GnomeSettingsTool::setD(const QString &schema, const QString &key, double value)
{
    setS(schema, key, QString::number(value, 'f', 6));
}
```

**Problems:**
1. Return type is `void` -- callers cannot distinguish success from failure.
2. The `catch` block only logs to `qCritical()`. Even if `CommandUtil::exec()` *did* throw (which it currently doesn't for exit-code failures), the error is caught and silently consumed.
3. `setB`, `setI`, `setD` all delegate to `setS` and inherit the same void-return, no-propagation pattern.

**macOS variant** (`macos/nexis-core/Tools/gnome_settings_tool.cpp`): Identical pattern. Uses `defaults write` instead of `gsettings set`, but has the same void-return, catch-and-log-only behavior. On macOS, `setB`, `setI`, `setD` have their own try/catch blocks (they don't delegate to `setS`) but all have the same problem.

### Layer 3: Tab Lambda/Slot Connections

All four tabs follow an identical pattern. The constructor connects widget signals to lambdas that call `GnomeSettingsTool::set*()` as a fire-and-forget void call with no return value check.

#### GnomeAppearanceTab (shared/nexis/Pages/GnomeSettings/gnome_appearance_tab.cpp)

**Widget types and their set* calls (15 connections):**

| Widget | Signal | set* call | Can revert? |
|--------|--------|-----------|-------------|
| `cmbColorScheme` (QComboBox) | `currentIndexChanged` | `setS(INTERFACE, COLOR_SCHEME, ...)` | No |
| `editGtkTheme` (QLineEdit) | `editingFinished` | `setS(INTERFACE, GTK_THEME, ...)` | No |
| `editIconTheme` (QLineEdit) | `editingFinished` | `setS(INTERFACE, ICON_THEME, ...)` | No |
| `editCursorTheme` (QLineEdit) | `editingFinished` | `setS(INTERFACE, CURSOR_THEME, ...)` | No |
| `spinCursorSize` (QSpinBox) | `valueChanged` | `setI(INTERFACE, CURSOR_SIZE, ...)` | No |
| `editFont` (QLineEdit) | `editingFinished` | `setS(INTERFACE, FONT_NAME, ...)` | No |
| `editDocFont` (QLineEdit) | `editingFinished` | `setS(INTERFACE, DOCUMENT_FONT, ...)` | No |
| `editMonoFont` (QLineEdit) | `editingFinished` | `setS(INTERFACE, MONOSPACE_FONT, ...)` | No |
| `spinTextScaling` (QDoubleSpinBox) | `valueChanged` | `setD(INTERFACE, TEXT_SCALING, ...)` | No |
| `chkAnimations` (QCheckBox) | `toggled` | `setB(INTERFACE, ENABLE_ANIMATIONS, ...)` | No |
| `chkHotCorners` (QCheckBox) | `toggled` | `setB(INTERFACE, ENABLE_HOT_CORNERS, ...)` | No |
| `chkClockSeconds` (QCheckBox) | `toggled` | `setB(INTERFACE, CLOCK_SECONDS, ...)` | No |
| `chkClockWeekday` (QCheckBox) | `toggled` | `setB(INTERFACE, CLOCK_WEEKDAY, ...)` | No |
| `chkBatteryPct` (QCheckBox) | `toggled` | `setB(INTERFACE, SHOW_BATTERY_PCT, ...)` | No |
| `cmbClockFormat` (QComboBox) | `currentIndexChanged` | `setS(INTERFACE, CLOCK_FORMAT, ...)` | No |
| `cmbAntialiasing` (QComboBox) | `currentIndexChanged` | `setS(INTERFACE, FONT_ANTIALIASING, ...)` | No |
| `cmbHinting` (QComboBox) | `currentIndexChanged` | `setS(INTERFACE, FONT_HINTING, ...)` | No |

Example lambda pattern:
```cpp
connect(ui->cmbColorScheme, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
    if (mLoading) return;
    GnomeSettingsTool::setS(GnomeSchema::INTERFACE, GnomeKey::COLOR_SCHEME,
                            ui->cmbColorScheme->currentData().toString());
});
```

#### GnomeWmTab (shared/nexis/Pages/GnomeSettings/gnome_wm_tab.cpp)

**12 connections:**

| Widget | Signal | set* call |
|--------|--------|-----------|
| `editButtonLayout` (QLineEdit) | `editingFinished` | `setS(WM_PREFS, BUTTON_LAYOUT, ...)` |
| `cmbFocusMode` (QComboBox) | `currentIndexChanged` | `setS(WM_PREFS, FOCUS_MODE, ...)` |
| `editTitlebarFont` (QLineEdit) | `editingFinished` | `setS(WM_PREFS, TITLEBAR_FONT, ...)` |
| `spinWorkspaces` (QSpinBox) | `valueChanged` | `setI(WM_PREFS, NUM_WORKSPACES, ...)` |
| `cmbDblClick` (QComboBox) | `currentIndexChanged` | `setS(WM_PREFS, ACTION_DBL_CLICK, ...)` |
| `cmbMidClick` (QComboBox) | `currentIndexChanged` | `setS(WM_PREFS, ACTION_MID_CLICK, ...)` |
| `cmbRightClick` (QComboBox) | `currentIndexChanged` | `setS(WM_PREFS, ACTION_RIGHT_CLICK, ...)` |
| `chkAutoRaise` (QCheckBox) | `toggled` | `setB(WM_PREFS, AUTO_RAISE, ...)` |
| `chkRaiseOnClick` (QCheckBox) | `toggled` | `setB(WM_PREFS, RAISE_ON_CLICK, ...)` |
| `chkDynamicWorkspaces` (QCheckBox) | `toggled` | `setB(MUTTER, DYNAMIC_WORKSPACES, ...)` |
| `chkEdgeTiling` (QCheckBox) | `toggled` | `setB(MUTTER, EDGE_TILING, ...)` |
| `chkAutoMaximize` (QCheckBox) | `toggled` | `setB(MUTTER, AUTO_MAXIMIZE, ...)` |
| `chkCenterNewWindows` (QCheckBox) | `toggled` | `setB(MUTTER, CENTER_NEW_WINDOWS, ...)` |
| `chkWorkspacesPrimary` (QCheckBox) | `toggled` | `setB(MUTTER, WORKSPACES_PRIMARY, ...)` |

#### GnomeMouseTab (shared/nexis/Pages/GnomeSettings/gnome_mouse_tab.cpp)

**8 connections:**

| Widget | Signal | set* call |
|--------|--------|-----------|
| `chkMouseNatural` (QCheckBox) | `toggled` | `setB(MOUSE, NATURAL_SCROLL, ...)` |
| `sliderMouseSpeed` (QSlider) | `valueChanged` | `setD(MOUSE, SPEED, ...)` |
| `cmbAccelProfile` (QComboBox) | `currentIndexChanged` | `setS(MOUSE, ACCEL_PROFILE, ...)` |
| `chkLeftHanded` (QCheckBox) | `toggled` | `setB(MOUSE, LEFT_HANDED, ...)` |
| `chkTapToClick` (QCheckBox) | `toggled` | `setB(TOUCHPAD, TAP_TO_CLICK, ...)` |
| `chkTouchpadNatural` (QCheckBox) | `toggled` | `setB(TOUCHPAD, NATURAL_SCROLL, ...)` |
| `sliderTouchpadSpeed` (QSlider) | `valueChanged` | `setD(TOUCHPAD, SPEED, ...)` |
| `chkTwoFingerScroll` (QCheckBox) | `toggled` | `setB(TOUCHPAD, TWO_FINGER_SCROLL, ...)` |
| `chkEdgeScrolling` (QCheckBox) | `toggled` | `setB(TOUCHPAD, EDGE_SCROLLING, ...)` |
| `chkDisableTyping` (QCheckBox) | `toggled` | `setB(TOUCHPAD, DISABLE_TYPING, ...)` |

Note: The slider lambdas also update a display label (`lblMouseSpeedVal` / `lblTouchpadSpeedVal`) before calling `setD()`:
```cpp
connect(ui->sliderMouseSpeed, &QSlider::valueChanged, this, [this](int val) {
    if (mLoading) return;
    double speed = val / 100.0;
    ui->lblMouseSpeedVal->setText(QString::number(speed, 'f', 2));
    GnomeSettingsTool::setD(GnomeSchema::MOUSE, GnomeKey::SPEED, speed);
});
```
On failure, the label also needs to be reverted (not just the slider value).

#### GnomeDesktopTab (shared/nexis/Pages/GnomeSettings/gnome_desktop_tab.cpp)

**8 connections:**

| Widget | Signal | set* call | Notes |
|--------|--------|-----------|-------|
| `editWallpaper` (QLineEdit) | `editingFinished` | `setS(BACKGROUND, PICTURE_URI, ...)` | |
| `btnBrowseWallpaper` (QPushButton) | `clicked` | `setS(BACKGROUND, PICTURE_URI, ...)` | Also sets editWallpaper text |
| `editWallpaperDark` (QLineEdit) | `editingFinished` | `setS(BACKGROUND, PICTURE_URI_DARK, ...)` | |
| `btnBrowseWallpaperDark` (QPushButton) | `clicked` | `setS(BACKGROUND, PICTURE_URI_DARK, ...)` | Also sets editWallpaperDark text |
| `cmbPictureOptions` (QComboBox) | `currentIndexChanged` | `setS(BACKGROUND, PICTURE_OPTIONS, ...)` | |
| `chkEventSounds` (QCheckBox) | `toggled` | `setB(SOUND, EVENT_SOUNDS, ...)` | |
| `chkInputFeedback` (QCheckBox) | `toggled` | `setB(SOUND, INPUT_FEEDBACK, ...)` | |
| `chkVolumeOver100` (QCheckBox) | `toggled` | `setB(SOUND, VOLUME_OVER_100, ...)` | |

Note: The `btnBrowseWallpaper` and `btnBrowseWallpaperDark` connections set both the QLineEdit text AND call `setS()`. They do NOT have the `mLoading` guard (file dialogs can't be triggered during loading). On failure, both the line edit text and the gsetting need to revert.

### Layer 4: The `mLoading` Guard Pattern

Each tab has a `bool mLoading` member. In `loadSettings()`:
```cpp
void GnomeAppearanceTab::loadSettings()
{
    mLoading = true;
    // ... populate all widgets from GnomeSettingsTool::get*() ...
    mLoading = false;
}
```

Every lambda begins with `if (mLoading) return;` to prevent the initial widget population from triggering write-back loops.

**Relevance to the fix:** When reverting a widget on failure, we must temporarily set `mLoading = true` (or use `QSignalBlocker`) to prevent the revert itself from triggering another `set*()` call. Without this, reverting a QCheckBox's `setChecked()` would fire `toggled()` again, which would call `setB()` again with the old value, which would succeed (since it's the valid current value), creating an unnecessary subprocess call.

### Layer 5: Header Files / Class Structure

All four tab headers follow the same pattern:
```cpp
class GnomeAppearanceTab : public QWidget
{
    Q_OBJECT
public:
    explicit GnomeAppearanceTab(QWidget *parent = nullptr);
    ~GnomeAppearanceTab();
private:
    void loadSettings();
    Ui::GnomeAppearanceTab *ui;
    bool mLoading;
};
```

No signals are emitted. No error handling infrastructure exists. The parent `GnomeSettingsPage` is a simple container that manages tab switching via `QStackedWidget` -- it has no error display mechanisms either.

### Layer 6: Main Page (shared/nexis/Pages/GnomeSettings/gnome_settings_page.cpp)

Simple container. Creates the four tab widgets, adds them to a `QStackedWidget`, and wires up tab button clicks. No error handling, no status bar, no notification mechanism.

The `.ui` file shows a `QVBoxLayout` with a horizontal tab bar row and a `QStackedWidget` below. No status label or error area exists.

---

## How `gsettings set` Can Fail

Real-world failure scenarios:

1. **Invalid value for the key's type:** `gsettings set org.gnome.desktop.interface cursor-size "not-a-number"` -- returns exit code 1, stderr: `"Expected a value of type 'i' but got 'not-a-number'"`
2. **Schema doesn't exist:** `gsettings set org.nonexistent.schema key val` -- returns exit code 1
3. **Key doesn't exist in schema:** `gsettings set org.gnome.desktop.interface nonexistent-key val` -- returns exit code 1
4. **Key is locked by admin (dconf lockdown):** `gsettings set ...` -- returns exit code 1, stderr indicates the key is locked
5. **dconf/dbus errors:** D-Bus session bus unavailable, dconf database corruption
6. **Permission issues:** Running as user that can't write to dconf database

---

## Existing Error Feedback Patterns in the Codebase

The project uses `QMessageBox` for confirmations and warnings elsewhere:
- `shared/nexis/main.cpp:76` -- duplicate instance warning
- `shared/nexis/Pages/Uninstaller/uninstaller_page.cpp:256` -- confirm uninstall dialog
- `shared/nexis/Pages/AptSourceManager/apt_source_manager_page.cpp:323` -- confirm uninstall dialog
- `shared/nexis/Pages/Resources/disk_usage_launcher_widget.cpp:445` -- unsupported package manager warning

`QMessageBox` is the only feedback mechanism currently in use. There is no status bar, toast notification system, or inline error label pattern.

---

## Summary of What Must Change

### 1. `CommandUtil::exec()` must detect non-zero exit codes

Currently only checks `QProcess::error()` which is about process *launch* failures. Must also check `process->exitCode() != 0` and `process->exitStatus() != QProcess::NormalExit`. Should read stderr for the error message.

**Concern:** `CommandUtil::exec()` is used throughout the entire codebase (12+ call sites). Changing its behavior to throw on non-zero exit codes could break existing callers that rely on the current behavior of silently returning stdout even when the exit code is non-zero. Need to either:
- Add a parameter to opt-in to exit-code checking, OR
- Audit all existing callers to confirm they'd handle the new throw correctly, OR
- Leave `CommandUtil::exec()` as-is and add the exit-code check in `GnomeSettingsTool::set*()` specifically.

### 2. `GnomeSettingsTool::set*()` must return success/failure

Change from `void` to `bool` (or return a struct with error details). Both Linux and macOS implementations need updating.

### 3. Tab lambdas must check the return value and revert on failure

Each of the ~43 lambdas across 4 tabs needs to:
- Capture the widget's current value before calling `set*()`
- Call `set*()` and check the return
- On failure: use `QSignalBlocker` or `mLoading` guard to revert the widget without triggering a re-write
- Show user-visible feedback

### 4. User-visible error feedback mechanism

Options:
- **QMessageBox** -- consistent with existing codebase patterns but disruptive for rapid settings changes
- **Inline status label** -- a `QLabel` in the main `GnomeSettingsPage` that shows transient error messages (preferred for UX)
- **QToolTip or QStatusBar** -- less common in this codebase

---

## Widget Revert Complexity by Type

| Widget Type | Revert Method | Signal to Block |
|-------------|---------------|-----------------|
| QCheckBox | `setChecked(oldValue)` | `toggled` |
| QComboBox | `setCurrentIndex(oldIndex)` | `currentIndexChanged` |
| QSpinBox | `setValue(oldValue)` | `valueChanged` |
| QDoubleSpinBox | `setValue(oldValue)` | `valueChanged` |
| QSlider | `setValue(oldValue)` | `valueChanged` |
| QLineEdit | `setText(oldText)` | `editingFinished` (less of a concern since it's not auto-triggered by setText) |

**QSignalBlocker** is the cleanest approach for preventing re-fire:
```cpp
{
    const QSignalBlocker blocker(ui->chkAnimations);
    ui->chkAnimations->setChecked(oldValue);
}
```
This is preferable to toggling `mLoading` because it's RAII-based, exception-safe, and clearly scoped.

---

## Total Impact Count

- **Files to modify:** 7 (command_util_shared.cpp, gnome_settings_tool.h, linux gnome_settings_tool.cpp, macos gnome_settings_tool.cpp, and 4 tab .cpp files)
- **Possibly also:** gnome_settings_page.cpp/ui (if adding a status label)
- **Lambdas to update:** ~43 across 4 tabs
- **Platform consideration:** Both Linux (`gsettings`) and macOS (`defaults`) implementations need identical changes
