# FR-20: Docker Image / Volume Management — Implementation Plan

## Overview

Add a new "Docker" sidebar page to Nexis for managing Docker images, containers, and volumes. The page is conditionally shown only when Docker is installed. Follows the established Uninstaller page pattern (tree widget, multi-select, batch actions, async loading).

Cross-platform: Docker CLI is identical on macOS and Linux — single shared implementation, no platform-specific code.

---

## Phase 1 — Core Library: DockerTool

Create the backend tool that wraps Docker CLI commands and parses their output.

### Task 1.1: Data structures (`docker_tool.h`)
- [x] Create `shared/nexis-core/Tools/docker_tool.h`
- [x] Define structs:
  - `DockerImage` — id, repository, tag, size, sizeBytes (qint64), createdAt, isDangling, isUsed
  - `DockerContainer` — id, name, status, state (running/exited/paused), image, ports, createdAt
  - `DockerVolume` — name, driver, mountpoint, isUsed
- [x] Define `DockerTool` class with static methods (singleton not needed — stateless utility):
  - `static bool isDockerInstalled()` — checks `CommandUtil::isExecutable("docker")`
  - `static bool isDaemonRunning()` — runs `docker version` and checks exit code
  - `static QString dockerVersion()` — parses version string
  - `static QList<DockerImage> getImages()`
  - `static QList<DockerContainer> getContainers()`
  - `static QList<DockerVolume> getVolumes()`
  - `static bool removeImages(const QStringList &ids)`
  - `static bool removeContainers(const QStringList &ids)`
  - `static bool removeVolumes(const QStringList &names)`
  - `static int pruneImages()` — returns count removed
  - `static int pruneContainers()` — returns count removed
  - `static int pruneVolumes()` — returns count removed
  - `static bool startContainer(const QString &id)`
  - `static bool stopContainer(const QString &id)`

**Acceptance:** Header compiles. Structs have all fields needed by UI. ✅

### Task 1.2: Implementation (`docker_tool.cpp`)
- [x] Create `shared/nexis-core/Tools/docker_tool.cpp`
- [x] Implement detection methods
- [x] Implement `getImages()`
- [x] Implement `getContainers()`
- [x] Implement `getVolumes()`
- [x] Implement action methods

**Acceptance:** All methods return correct data when Docker is installed and running. Graceful empty results when Docker is absent or daemon is stopped. Build succeeds. ✅

### Task 1.3: Add DockerTool to ToolManager
- [x] Add `#include <Tools/docker_tool.h>` to `tool_manager.h`
- [x] Add `bool checkDocker() const;` method to ToolManager
- [x] Implement in both `linux/nexis/Managers/tool_manager.cpp` and `macos/nexis/Managers/tool_manager.cpp`

**Acceptance:** `ToolManager::ins()->checkDocker()` returns true when docker is in PATH. ✅

---

## Phase 2 — UI: Docker Page

### Task 2.1: Create page files
- [x] Create directory: `shared/nexis/Pages/Docker/`
- [x] Create `docker_page.h`
- [x] Create `docker_page.cpp` with full implementation (~510 lines)

### Task 2.2: Create UI layout (`docker_page.ui`)
- [x] Create `docker_page.ui` with QStackedWidget (loading/content/daemon-stopped), QTabWidget (3 tabs), action buttons

**Acceptance:** Page renders with 3 tabs, loading spinner, tree widgets with headers. Builds successfully. ✅

---

## Phase 3 — App Integration

### Task 3.1: Sidebar button in app.ui
- [x] Add `btnDocker` QPushButton to `app.ui` sidebar layout between btnResources and btnHelpers

### Task 3.2: Create sidebar icon
- [x] Create `docker.svg` in `shared/nexis/static/themes/default/img/sidebar-icons/`

### Task 3.3: Register page in app.h / app.cpp
- [x] In `app.h`: Add include, member, slot declaration
- [x] In `app.cpp::init()`: Add conditional registration block
- [x] Add label: `ui->btnDocker->setText(tr("Docker"));`
- [x] Add click slot: `void App::on_btnDocker_clicked()`
- [x] Add icon in `updateSidebarIcons()`

### Task 3.4: Update CMakeLists.txt
- [x] Add `"${GUI_SHARED_DIR}/Pages/Docker"` to `CMAKE_AUTOUIC_SEARCH_PATHS`
- [x] Add `"${GUI_SHARED_DIR}/Pages/Docker"` to `target_include_directories`

**Acceptance:** Docker button appears in sidebar when Docker is installed. Clicking it navigates to the Docker page. Button hidden when Docker is not installed. Build succeeds. ✅

---

## Phase 4 — Polish & Edge Cases

### Task 4.1: "Docker not running" state
- [x] QStackedWidget page 2 shows "Docker daemon is not running. Start Docker and click Refresh."
- [x] Status label with danger accessibleName for red color
- [x] Refresh button re-checks daemon status
- [x] Action buttons disabled when daemon is stopped

### Task 4.2: Permission handling
- [x] Docker CLI failures return empty results gracefully (CommandUtil::execWithStatus checks exit code)

### Task 4.3: Empty state
- [x] Tree sections only created when items exist; empty tabs show no sections

### Task 4.4: Confirmation dialogs
- [x] QMessageBox::warning for all remove and prune operations
- [x] Volume removal has extra-prominent "permanently lost" warning

### Task 4.5: Refresh after actions
- [x] All actions re-fetch data after completion via fetchImages/fetchContainers/fetchVolumes

**Acceptance:** All edge cases handled gracefully. ✅

---

## Phase 5 — Build & Test

### Task 5.1: Build verification
- [x] Clean rebuild succeeds on macOS with zero errors and zero warnings from Docker page

**Acceptance:** Build succeeds. ✅

---

## Files Created

| File | Description |
|------|-------------|
| `shared/nexis-core/Tools/docker_tool.h` | DockerTool class + data structs |
| `shared/nexis-core/Tools/docker_tool.cpp` | Docker CLI wrapper implementation |
| `shared/nexis/Pages/Docker/docker_page.h` | Page header |
| `shared/nexis/Pages/Docker/docker_page.cpp` | Page implementation |
| `shared/nexis/Pages/Docker/docker_page.ui` | Page layout |
| `shared/nexis/static/themes/default/img/sidebar-icons/docker.svg` | Sidebar icon |

## Files Modified

| File | Change |
|------|--------|
| `shared/nexis/app.h` | Add DockerPage include, member, slot |
| `shared/nexis/app.cpp` | Register page conditionally, add label/icon/click handler |
| `shared/nexis/app.ui` | Add `btnDocker` sidebar button |
| `shared/nexis/Managers/tool_manager.h` | Add `checkDocker()` method |
| `linux/nexis/Managers/tool_manager.cpp` | Implement `checkDocker()` |
| `macos/nexis/Managers/tool_manager.cpp` | Implement `checkDocker()` |
| `CMakeLists.txt` | Add Docker page to AUTOUIC_SEARCH_PATHS and include dirs |
| `FEATURE_REQUESTS.md` | Mark FR-20 done |
| `CHANGELOG.md` | Add entry for FR-20 |
