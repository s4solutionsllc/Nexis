# Bundle A — Cold Launch Sprint: Implementation Plan (FR-96, FR-97, FR-98)

Implementation plan for the three FRs in Bundle A. Derived from the research
artifact at `backlog/FR-96-98_research.md` — consult it for file paths,
line numbers, and architectural rationale.

**Order of execution:** FR-97 (scaffold, then flip) → FR-98 → FR-96 (extract, then fix-up). Five commits total; each compiles and passes tests on its own.

**Expected combined user-visible win:** 400-1800 ms faster time-to-interactive, ~50-120 MB lower idle RAM.

---

## Commit A — FR-97 scaffold (zero behavior change)

Refactor `mListPages` into a structure that associates each page with its factory, title, and a cached pointer. Keep *all* pages eagerly constructed on first run — this commit only changes the shape of the data structure so the subsequent flip commit is minimal and reviewable.

- [ ] **A.1** — Introduce `struct PageSlot` in `shared/nexis/app.h`:
  ```cpp
  struct PageSlot {
      QString title;                              // windowTitle() — used by palette + getPageByTitle
      std::function<QWidget*()> factory;          // constructs the page on demand
      QPointer<QWidget> widget = nullptr;         // cached after first construction
      std::function<void(QWidget*)> onConstructed; // optional post-construct hook (theme refresh, signals)
  };
  ```
- [ ] **A.2** — Replace `QList<QWidget*> mListPages;` with `QList<PageSlot> mPageSlots;` in `app.h`.
- [ ] **A.3** — Add accessor methods to `App`:
  - `QWidget* App::ensurePage(int index)` — returns the cached widget, or constructs via factory, stashes, runs `onConstructed`, adds to `mSlidingStacked`, then returns. Idempotent.
  - `QWidget* App::ensurePageByTitle(const QString& title)` — linear search in `mPageSlots` by title, calls `ensurePage(i)`.
  - Keep the existing `getPageByTitle()` signature as a wrapper around `ensurePageByTitle()` so external callers don't break.
- [ ] **A.4** — In `App::init()`, rewrite the page-construction block to push `PageSlot` entries into `mPageSlots` with factory lambdas, then immediately call `ensurePage(i)` for every slot. Net effect: identical behavior to today, but now going through the factory path.
- [ ] **A.5** — Update `App::setupCommandPalette()` to iterate `mPageSlots` by title — command callbacks invoke `clickSidebarButton(slot.title, true)` rather than capturing page pointers.
- [ ] **A.6** — Update `App::clickSidebarButton()` (the overload that takes a title) to drive `ensurePageByTitle()`.
- [ ] **A.7** — Audit all call sites of the old `mListPages`. Search: `grep -n mListPages shared/nexis/` — update each to iterate `mPageSlots` or call `ensurePage()`. Likely sites: sidebar wiring, kiosk show/hide, maintenance-wizard launcher.
- [ ] **A.8** — Build incrementally (`cmake --build build`), ensure zero test regressions (`ctest`), confirm splash-to-dashboard flow still works manually.

**Acceptance (Commit A):**
- All 13-16 pages still appear in the sidebar and stacked widget at app startup.
- Command palette still navigates correctly.
- `ScreenshotTests` unchanged pass rate vs. baseline.
- Diff to `shared/nexis/app.cpp` / `app.h` is mechanical; no visible behavior change.

---

## Commit B — FR-97 flip (pages become lazy)

Keep Dashboard eagerly constructed (default landing page and it subscribes to all `DataRefreshService` signals that other pages share). Defer construction of all other pages until first navigation.

- [ ] **B.1** — In `App::init()`, construct `DashboardPage` immediately via its factory and keep its slot's `widget` populated. All other slots — leave `widget == nullptr`, only populate `factory` and `title`.
- [ ] **B.2** — Move `mSlidingStacked->addWidget(page)` out of `App::init()` and into `App::ensurePage()` — a page is added to the stack on first construction, not up front.
- [ ] **B.3** — Populate each slot's `onConstructed` hook to run:
  - `mSlidingStacked->addWidget(widget)` (now in `ensurePage` itself, not the hook).
  - An explicit theme sync (call `emit SignalMapper::ins()->sigChangedAppTheme()` for that page's receiver, or invoke the page's `refreshThemeColors()` if it has one). This handles the "missed the startup theme signal" case.
- [ ] **B.4** — Update `App::setupCommandPalette()` to iterate `mPageSlots[i].title` — no page-pointer lookup needed. Each palette command callback calls `clickSidebarButton(title, true)` which triggers `ensurePage()`.
- [ ] **B.5** — Update `App::toggleKioskMode()` / `applyKioskMode()`: if kiosk mode is entered before a non-Dashboard page has been constructed, no change needed (kiosk hides everything except Dashboard anyway). Verify by reading `applyKioskMode()` and confirming it doesn't iterate `mPageSlots` for filtering.
- [ ] **B.6** — Fix `ScreenshotTests` regression (per research §6.6):
  - Change the test to force-navigate via the sidebar button click path before each screenshot, not direct `setCurrentWidget` — this exercises the lazy path end-to-end.
  - File: `tests/screenshots/test_screenshots.cpp` around `:112-121, 143`.
  - Alternative: expose `App::ensurePageByClassName(const char*)` as a test helper gated by `#ifdef QT_BUILD_INTERNAL` or a CMake flag. Prefer the force-navigation approach.
- [ ] **B.7** — Audit `DataRefreshService::ins()->start()` and confirm no page needs to exist at start-time for the service to function. Service is a singleton; pages subscribe on construction; service emits regardless. Safe.
- [ ] **B.8** — Audit `sigCleanableSizeChanged` (`SystemCleanerPage` emits it). If SystemCleanerPage is lazy, the sidebar badge won't update until the user visits System Cleaner once. Verify current behavior: the badge reflects the last-scanned size persisted across sessions? If yes, lazy construction is fine; if no, document the change.
- [ ] **B.9** — Build + test manually: launch app, navigate to each sidebar item in turn, confirm construction is visible (instrument with `qDebug` temporarily — remove before commit).
- [ ] **B.10** — Verify command palette: Ctrl+K → type each page name → confirm navigation constructs the page.

**Acceptance (Commit B):**
- `ScreenshotTests` passes with all pages captured.
- Manual cold launch: app usable before non-Dashboard pages finish their I/O.
- RAM footprint (via `ps -o rss`) after 10 s on Dashboard is measurably lower than pre-commit A (target ~50-120 MB less).

---

## Commit C — FR-98 HardwareInfoPage deferred populate

Move `HardwareInfoPage::init()`'s populate calls out of the constructor and into a one-shot `showEvent` handler.

- [ ] **C.1** — In `shared/nexis/Pages/HardwareInfo/hardware_info_page.h`:
  - Add `bool mPopulated = false;` member.
  - Override `void showEvent(QShowEvent* event) override;`.
- [ ] **C.2** — In `shared/nexis/Pages/HardwareInfo/hardware_info_page.cpp`:
  - Constructor: remove the `init()` call. Keep `ui->setupUi(this)` and the `sigChangedAppTheme` connect — these are cheap and needed before first show.
  - Implement `showEvent` that checks `!mPopulated`, runs `init()`, sets `mPopulated = true`, calls `QWidget::showEvent(event)` parent impl.
  - Rename `init()` → `populateAll()` for clarity (or leave as `init()` — preference).
- [ ] **C.3** — Verify: command palette-driven `clickSidebarButton("Hardware Info")` flow ends with `mSlidingStacked->setCurrentWidget(hwPage)` which fires `showEvent` — confirms population runs on first navigation.
- [ ] **C.4** — Verify: the theme-refresh slot (`refreshThemeColors`) is safe to call on an un-populated page (it iterates `mHealthItems` which is empty pre-populate; see research §5).
- [ ] **C.5** — Build + manual test: (1) launch app, do not click Hardware Info — confirm no sysctl / smartctl / battery I/O from that page. (2) Click Hardware Info — confirm page populates correctly. (3) Click another sidebar item and back to Hardware Info — confirm no re-populate (idempotent via `mPopulated`).
- [ ] **C.6** — Run screenshot tests — Hardware Info screenshots must still match (they are taken after the page is navigated to, per test harness).

**Acceptance (Commit C):**
- Cold launch: no hardware-info I/O until user visits the page.
- First visit to Hardware Info: population completes (may take a noticeable moment on older machines — acceptable, was baseline cost before).
- Second+ visit: instant.
- All screenshot tests pass.

---

## Commit D — FR-96 extract (async disk-health discovery)

Extract the `discoverDrives()` call out of the `DiskHealthInfo*` constructors. Introduce a post-`App::show()` async trigger. This commit may leave one-shot `hasDiskHealth()` readers temporarily stale — Commit E patches them.

- [ ] **D.1** — In `shared/nexis-core/Info/disk_health_info_shared.h` (or equivalent):
  - Ensure `discoverDrives()` is a public method (or add a public `initAsync()` wrapper that calls it).
  - Add/confirm `bool DiskHealthInfo::isDiscoveryComplete()` — returns true once `discoverDrives()` has returned at least once.
- [ ] **D.2** — In both `macos/nexis-core/Info/disk_health_info.cpp:76-80` and `linux/nexis-core/Info/disk_health_info.cpp:8-12`:
  - Remove the `discoverDrives()` call from the constructor.
  - Keep `CommandUtil::isExecutable("smartctl")` — cheap and needed for the later discovery gate.
- [ ] **D.3** — Add a new method `InfoManager::kickOffDeferredInit()` in `shared/nexis/Managers/info_manager.cpp`:
  ```cpp
  void InfoManager::kickOffDeferredInit() {
      if (mDeferredInitStarted.exchange(true)) return;  // atomic bool; once only
      QtConcurrent::run([this]() {
          dhi->discoverDrives();
          // Emit via DataRefreshService on UI thread so existing connections receive it.
          QMetaObject::invokeMethod(DataRefreshService::ins(), [] {
              DataRefreshService::ins()->refreshHealthAndEmit();
          }, Qt::QueuedConnection);
      });
  }
  ```
  - Add `std::atomic<bool> mDeferredInitStarted{false};` to `InfoManager` private section.
- [ ] **D.4** — In `DataRefreshService`, ensure `refreshHealthAndEmit()` exists (or rename the body of `onSlowTick()` that calls `refreshHealth()` + `emit diskHealthUpdated(...)` into a reusable helper). The slow tick timer continues to call it every 30 s.
- [ ] **D.5** — In `shared/nexis/main.cpp` right after `w.show()`:
  ```cpp
  QTimer::singleShot(0, &w, [](){
      InfoManager::ins()->kickOffDeferredInit();
  });
  ```
  The `singleShot(0)` queues the work for the first event-loop turn — after the window has painted and the splash has closed.
- [ ] **D.6** — Build + test: cold launch, confirm (a) splash disappears faster, (b) `qDebug` instrumentation shows `discoverDrives()` running on a non-main thread, (c) `diskHealthUpdated` signal emits within 1-2 s after app shows.

**Acceptance (Commit D):**
- `InfoManager::ins()` construction time is sub-10 ms (no more blocking I/O).
- DiskTile on Dashboard shows placeholder briefly, then fills in.
- Dashboard paints at least ~500-1500 ms sooner than pre-commit-D on machines with external drives.

---

## Commit E — FR-96 fix-up (re-evaluate one-shot readers)

The readers listed in research §4/§6.4 make one-shot decisions at construction time based on `hasDiskHealth()`. After commit D, these may be made *before* discovery finishes. Patch each to re-evaluate on the first `diskHealthUpdated` signal.

- [ ] **E.1** — `ResourcesPage` (`shared/nexis/Pages/Resources/resources_page.cpp:45-55`):
  - Move the "create disk-temp chart" logic into a helper `ResourcesPage::ensureDiskTempChart()`.
  - Call it once in the constructor (no-op if no drives yet).
  - Connect to `DataRefreshService::diskHealthUpdated` with a slot that calls `ensureDiskTempChart()` — on first receipt, if drives with temp sensors exist and the chart was not yet created, create it now, add it to the scroll layout, and forward subsequent `diskHealthUpdated` signals to its `onDiskHealthUpdated` slot.
- [ ] **E.2** — `SettingsPage` (`shared/nexis/Pages/Settings/settings_page.cpp:173`):
  - The "disk-health alert" checkbox hide logic runs once at construction. If `hasDiskHealth()` is false at that moment but true later, the checkbox stays hidden.
  - Fix: in the constructor, connect `DataRefreshService::diskHealthUpdated` to a lambda that sets `checkbox->setVisible(!drives.isEmpty())`. Keep the initial hide as-is for the "truly no SMART support" case.
- [ ] **E.3** — `DashboardPage::init` Health Score calculator (`shared/nexis/Pages/Dashboard/dashboard_page.cpp:307`):
  - `calc->setComponentAvailable("smart", im->hasDiskHealth())` — runs once.
  - Fix: in `DashboardPage::onDiskHealthUpdated` (or `onHealthDiskHealthUpdated`), re-call `setComponentAvailable("smart", !drives.isEmpty())` on first data arrival, before applying scores. Already mostly aligned per research §6.4 — just verify and add the call if missing.
- [ ] **E.4** — `HardwareInfoPage::populateStorage` (moved to first-show by FR-98): on first visit after commit D, the async discovery may still be in flight. Acceptable — the page re-queries on `diskHealthUpdated`. Verify the existing "populate storage → signal-driven refresh" path by reading `populateStorage` (`:420-625`) and confirming it connects to `diskHealthUpdated` for updates.
- [ ] **E.5** — Validate: with no external drives attached, cold launch → Dashboard → confirm disk tile populates within ~1 s of splash close. With a USB drive attached (or simulated slowness), cold launch → confirm app is fully interactive while smartctl runs in the background.
- [ ] **E.6** — `ScreenshotTests` may need a brief `QTest::qWait(2000)` between `App::show()` and screenshot capture to give async discovery time to complete. Check current wait duration (`tests/screenshots/test_screenshots.cpp` around `:112`).

**Acceptance (Commit E):**
- All pages render correct health/disk info after async discovery lands.
- No visual regression on Resources disk-temp chart (it appears once drives are discovered).
- Health Score tile includes SMART weighting once drives are discovered.
- `ScreenshotTests` all pass.

---

## Final verification (post-Commit E)

- [ ] **F.1** — Incremental build from clean: `rm -rf build && cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=$(brew --prefix qt@6) && cmake --build build -j$(sysctl -n hw.ncpu)`.
- [ ] **F.2** — Full CTest: `ctest --test-dir build --output-on-failure`. Expect all 26 tests passing (including `ScreenshotTests` after B.6 adjustment).
- [ ] **F.3** — Manual timing smoke test: temporary `QElapsedTimer` in `main.cpp` (per research §8.1) — record delta for the following three scenarios:
  - Cold launch, no external drives (MacBook).
  - Cold launch, with USB-C external SSD attached.
  - Cold launch, with spinning-rust Time Machine drive attached.
  Compare to baseline numbers taken before Commit A. Record in the FR Resolved note.
- [ ] **F.4** — Remove all `qDebug` / `QElapsedTimer` instrumentation before final commit.
- [ ] **F.5** — Update `CHANGELOG.md` under the next version section (likely `[2.2.16]`):
  ```markdown
  ### Changed
  - **Cold launch (FR-96, FR-97, FR-98):** App launches up to ~1.5 s faster, particularly on systems with external drives. Sidebar pages are now constructed on-demand instead of all at startup, and disk-health SMART discovery runs in the background after the window appears.
  ```
- [ ] **F.6** — Update `docs/APPLICATION_OVERVIEW.md` (if it documents eager page construction) and `docs/ARCHITECTURE_REVIEW.md` (add note to page-lifecycle section about lazy construction).
- [ ] **F.7** — Update `FEATURE_REQUESTS.md`: mark FR-96, FR-97, FR-98 with `[x]` and `**Resolved:** (commit_hashes) ...` notes. Write the notes per the project convention from FR-94 / FR-95.
- [ ] **F.8** — Create `backlog/FR-96-98_uat.md` (Phase 4) with plain-language test cases for the user to validate.

---

## Risks summary (reference — see research §6 for full detail)

| # | Risk | Mitigation |
|---|---|---|
| 1 | Command palette breaks when pages are lazy | `App::ensurePageByTitle` + palette registers title-keyed callbacks (B.4) |
| 2 | `ScreenshotTests` silently skips lazy pages | Force-navigate via sidebar click path (B.6) |
| 3 | One-shot `hasDiskHealth()` readers get stale decisions | Fix-up pass in Commit E re-evaluates on first signal |
| 4 | Lazy pages miss `sigChangedAppTheme` fired at init | Explicit theme-sync hook in `onConstructed` (B.3) |
| 5 | Manual test regression undetected | F.3 scripted 3-scenario timing comparison |

---

## Out of scope (explicitly)

- **Preloading pages in idle time** (e.g. "after 5 s, construct the 2 most-likely-next pages"). Not doing this — complexity for uncertain gain. Revisit if users report "first click feels slow" after Bundle A ships.
- **Splash screen timing tweaks.** The splash pixmap shows before `App` is constructed; it disappears when `splash->finish(&w)` is called. Bundle A does not touch splash code.
- **Any `DataRefreshService` cadence changes.** Those belong to Bundle C (FR-103 / FR-104 / FR-105).
- **Any `CommandUtil::execAsync` introduction.** That belongs to Bundle B (FR-99).

---

*End of plan. Ready for review.*
