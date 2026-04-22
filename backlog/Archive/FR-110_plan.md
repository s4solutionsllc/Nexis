# FR-110 Plan — Boot-time Analyzer

## Tasks

### Phase 1 — Core Data Layer

- [ ] **1a.** Create `shared/nexis-core/Info/boot_analysis_info.h`
  - `BootEntry` struct: `name`, `durationMs`, `impact`
  - `BootAnalysisData` struct: `available`, `error`, `totalBootMs`, `QList<BootEntry> entries`
  - Abstract `BootAnalysisInfo` base class with `virtual BootAnalysisData analyze() const = 0`
  - Static helper `impactFor(double ms)` → "High" / "Medium" / "Low"

- [ ] **1b.** Create `linux/nexis-core/Info/boot_analysis_info_linux.h`
  - `BootAnalysisInfoLinux : public BootAnalysisInfo`

- [ ] **1c.** Create `linux/nexis-core/Info/boot_analysis_info.cpp`
  - Run `systemd-analyze` → parse total boot time
  - Run `systemd-analyze blame` → parse per-service entries
  - Return `available=false` with error if systemd-analyze not found

- [ ] **1d.** Create `macos/nexis-core/Info/boot_analysis_info_macos.h`
  - `BootAnalysisInfoMacOS : public BootAnalysisInfo`

- [ ] **1e.** Create `macos/nexis-core/Info/boot_analysis_info.cpp`
  - Run `sysctl kern.boottime` → compute total boot time (uptime)
  - Return empty entries with note about per-service unavailability

- [ ] **Build check 1:** `cmake --build build -j$(nproc)` — core layer only (not yet wired into CMakeLists)

### Phase 2 — GUI Page

- [ ] **2a.** Create `shared/nexis/Pages/BootAnalysis/boot_analysis_page.h`
  - Inherits `QWidget`
  - Private: `QTableWidget`, `QLabel` (title/subtitle/status), `QPushButton` (refresh)
  - Private: `QFuture<BootAnalysisData>`, `QAtomicInt mCancelled`
  - Slots: `onRefresh()`, `onAnalysisDone(BootAnalysisData)`
  - Member: `std::unique_ptr<BootAnalysisInfo> mInfo`

- [ ] **2b.** Create `shared/nexis/Pages/BootAnalysis/boot_analysis_page.cpp`
  - Constructor builds layout programmatically (QVBoxLayout → header row + table + status)
  - `onRefresh()`: disable button, set status "Analyzing…", run `QtConcurrent::run`
  - On completion: populate table, update subtitle, re-enable button
  - Table: 3 cols (Name, Duration, Impact), header stretch on Name col
  - Destructor: cancel any in-flight future

### Phase 3 — SVG Icons

- [ ] **3a.** Create `shared/nexis/static/themes/default/img/sidebar-icons/boot-analysis.svg` (white fill)
- [ ] **3b.** Create `shared/nexis/static/themes/light/img/sidebar-icons/boot-analysis.svg` (dark fill)

### Phase 4 — CMakeLists Wiring

- [ ] **4a.** Add `boot_analysis_info.h` to `CORE_SHARED_HDRS`
- [ ] **4b.** Add `boot_analysis_info_linux.h` and `boot_analysis_info.cpp` to Linux `CORE_PLAT_SRCS`/`CORE_PLAT_HDRS`
- [ ] **4c.** Add `boot_analysis_info_macos.h` and `boot_analysis_info.cpp` to macOS `CORE_PLAT_SRCS`/`CORE_PLAT_HDRS`
- [ ] **4d.** Add `boot_analysis_page.cpp` to `GUI_SHARED_SRCS`
- [ ] **4e.** Add `boot_analysis_page.h` to `GUI_SHARED_HDRS`
- [ ] **4f.** Add `${GUI_SHARED_DIR}/Pages/BootAnalysis` to `GUI_SHARED_INCLUDE_DIRS`

- [ ] **Build check 2:** `cmake --build build -j$(nproc)` — should compile all new files

### Phase 5 — App Registration

- [ ] **5a.** In `app.h`: add `#include "Pages/BootAnalysis/boot_analysis_page.h"` and `BootAnalysisPage *bootAnalysisPage;` member and `QPushButton *btnBootAnalysis;`
- [ ] **5b.** In `app.cpp buildSidebar()`: add `btnBootAnalysis` after `btnStartupApps` in MANAGE section
- [ ] **5c.** In `app.cpp` mPageSlots: add Boot Analysis slot after Startup Apps
- [ ] **5d.** In `app.cpp` mListSidebarButtons: add `btnBootAnalysis` after `btnStartupApps`
- [ ] **5e.** In `app.cpp` click handlers: connect `btnBootAnalysis` via `navByTitle(tr("Boot Analysis"))`
- [ ] **5f.** In `app.cpp updateSidebarIcons()`: add `setIcon(btnBootAnalysis, "boot-analysis.svg")`

- [ ] **Build check 3 (full):** `cmake --build build -j$(nproc)` — full app

### Phase 6 — Tests & Docs

- [ ] **6a.** Run `ctest --test-dir build --output-on-failure`
- [ ] **6b.** Update `docs/APPLICATION_OVERVIEW.md`
- [ ] **6c.** Update `FEATURE_REQUESTS.md`: mark `[x]`, add resolution note + commit hash
- [ ] **6d.** Commit: `feat(startup): boot-time analyzer page (FR-110)`
- [ ] **6e.** Move research/plan files to `backlog/Archive/`

## Acceptance Criteria
- Linux: page shows total boot time + ranked table of services with impact labels
- Linux (no systemd): page shows "systemd-analyze not available" gracefully
- macOS: page shows total uptime-since-boot, empty table with explanatory note
- Refresh button re-runs the analysis asynchronously without freezing the UI
- No hardcoded colors; no regressions to existing pages
