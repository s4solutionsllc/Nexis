# BUG-37 Research: System Cleaner scanLoading.gif animation not playing

## Problem

The animated loading GIF (`scanLoading.gif`) shown after clicking Scan on the System Cleaner page does not animate. The GIF file has been confirmed as multi-frame.

## Root Cause Analysis

### Issue 1: QMovie lifecycle — leak and stale state

In `system_cleaner_page.cpp` lines 76–88, inside the `sigChangedAppTheme` lambda:

```cpp
connect(SignalMapper::ins(), &SignalMapper::sigChangedAppTheme, [=] {
    QString themeName = AppManager::ins()->resolveThemeName();
    mLoadingMovie = new QMovie(...);       // NEW allocation every theme change
    ui->lblLoadingScanner->setMovie(mLoadingMovie);
    mLoadingMovie->start();
    ui->lblLoadingScanner->hide();
    // ... same for mLoadingMovie_2
});
```

Problems:
1. **Memory leak**: Each theme change allocates a `new QMovie` without `delete`-ing the old one. The old movie is orphaned (parent is `this`, so it survives until page destruction, but it's wasteful).
2. **Stale animation state**: `start()` is called immediately at init time, but the label is hidden. By the time the user clicks Scan (potentially minutes later), the movie's internal timer may have drifted or the animation could appear frozen on frame 0 because Qt's QMovie only advances frames when the widget is visible and being painted.

### Issue 2: Delayed initialisation — nullptr at click time

In the constructor (lines 21–22):
```cpp
mLoadingMovie(nullptr),
mLoadingMovie_2(nullptr)
```

The movies remain `nullptr` until `sigChangedAppTheme` fires. This signal is emitted from `AppManager::updateStylesheet()` during `App::init()`. If anything triggers `on_btnScan_clicked()` before the signal has fired, `mLoadingMovie` is null, and `setMovie(nullptr)` was never called, so the label shows nothing.

### Issue 3: No `start()` at show-time

In `on_btnScan_clicked()` (line 388):
```cpp
ui->lblLoadingScanner->show();
```

There is no `mLoadingMovie->start()` call here. The movie was started once at theme-init time but may not be actively running when the label becomes visible. Qt's QMovie documentation states that calling `start()` ensures the animation begins from the current frame. Without it, the movie may be in `QMovie::NotRunning` or `QMovie::Paused` state.

Same issue in `on_btnClean_clicked()` for `mLoadingMovie_2` / `lblLoadingCleaner`.

## Signal/Slot Flow

```
Constructor → mLoadingMovie = nullptr
    ↓
App::init() → AppManager::updateStylesheet()
    ↓ emits sigChangedAppTheme
Lambda → mLoadingMovie = new QMovie(...); start(); hide()
    ↓
User clicks btnScan
    ↓
on_btnScan_clicked() → lblLoadingScanner->show()  ← NO start() call
    ↓
QtConcurrent::run → systemScan() (worker thread)
    ↓
scanFinishedS → onScanFinished() → stackedWidget page change (hides label implicitly)
```

## Files

| File | Lines | What |
|------|-------|------|
| `system_cleaner_page.h` | 73–74 | `QMovie *mLoadingMovie, *mLoadingMovie_2` declarations |
| `system_cleaner_page.cpp` | 21–22 | Initialised to `nullptr` in constructor |
| `system_cleaner_page.cpp` | 76–88 | Movies created in `sigChangedAppTheme` lambda |
| `system_cleaner_page.cpp` | 388 | `lblLoadingScanner->show()` — no `start()` |
| `system_cleaner_page.cpp` | 408+ | `on_btnClean_clicked()` — same pattern for `lblLoadingCleaner` |
| `static.qrc` | 15, 35 | GIF registered for default and light themes |

## Fix Strategy

1. **Pre-initialise** both QMovie objects in the constructor using the default theme.
2. **On theme change**, update the movie's filename (`setFileName()`) and restart, instead of allocating new objects.
3. **Call `start()`** immediately before `show()` in both `on_btnScan_clicked()` and `on_btnClean_clicked()` to guarantee active animation.
4. **Call `stop()`** when hiding the labels (in `onScanFinished()` and `onCleanFinished()`) to conserve resources.
