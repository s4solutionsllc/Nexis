Perfect! Now I have all the information I need. Let me create a comprehensive research report.

## FR-20: Docker Image / Volume Management — Comprehensive Research Report

### Executive Summary

FR-20 addresses adding Docker container, image, and volume management capabilities to Nexis. This feature request originated from Stacer issue #454. Docker management is a medium-priority feature that aligns with Nexis's emerging "developer-focused cleaning" niche, as containerization on desktops is growing but remains underserved by system optimizers.

---

## 1. PROJECT ARCHITECTURE & PATTERNS

### 1.1 Page Architecture Pattern

All pages in Nexis follow a consistent three-file pattern:

**Files per page:**
- `page_name_page.h` — Header with Q_OBJECT, slots/signals, UI pointer, data structures
- `page_name_page.cpp` — Implementation with initialization, data fetching, UI updates
- `page_name_page.ui` — Qt Designer XML layout defining widgets, buttons, signals

**Example: Services page structure**
```
shared/nexis/Pages/Services/
├── services_page.h       # Q_OBJECT, signals, slots, QList<Service> mServices
├── services_page.cpp     # init(), getServices(), loadServices(), filter logic
└── services_page.ui      # QListWidget (custom ServiceItem widgets), combos for filtering
```

**Key lifecycle patterns:**
1. Constructor calls `init()` in derived class
2. `init()` sets up UI widgets, connects signals/slots, spawns background load via `QtConcurrent::run()`
3. Worker thread fetches data (I/O only, no UI access)
4. Worker thread emits signal when complete
5. Main thread slot receives signal and updates UI
6. User interacts with filters/actions, triggering additional slots

**Thread safety approach:**
- Background fetching uses `QtConcurrent::run()` and `QFuture<void>`
- Data stored in member variable (e.g., `QList<Service> mServices`)
- UI updates happen only on main thread via signal/slot connections
- On exit, `app.cpp::closeEvent()` calls `QThreadPool::globalInstance()->waitForDone()` to finish background tasks (BUG-05)

---

### 1.2 Reference Pages: Uninstaller & Services

#### **Uninstaller Page** (nexis/Pages/Uninstaller/)
Best template for Docker management due to "list + action" pattern:

**UI Layout:**
- Top: Search field (`txtPackageSearch`) with live filtering
- Middle: `QTreeWidget` grouped by section/category with checkboxes for multi-select
- Bottom: "Uninstall" button, status count label, stacked widget (loading spinner vs. list)
- macOS has "Purge" checkbox; Linux/Windows may differ by context

**Data Model:**
```cpp
struct Package {
    QString name;
    QString description;
    QString section;       // Category like "System", "Development", "Games"
    QString path;          // Full filesystem path (macOS .app bundles only)
};
```

**Concurrency Pattern:**
```cpp
// In init():
mFetchFuture = QtConcurrent::run([this]() { fetchPackages(); });
connect(this, &UninstallerPage::packagesLoadedS, this, &UninstallerPage::onPackagesLoaded);

// Worker thread:
void fetchPackages() {
    mPackages = tm->getPackages();
    emit packagesLoadedS();  // Signal main thread
}

// Main thread slot:
void onPackagesLoaded() {
    // Clear and rebuild QTreeWidget from mPackages
    // Show/hide "not found" widget based on list size
}
```

**Action Execution:**
- User selects items via checkboxes
- Click "Uninstall" → `on_btnUninstall_clicked()`
- Helper collects selected items: `getSelectedPackages()`
- Passes to ToolManager: `tm->uninstallPackages(selected, purge)`
- ToolManager delegates to platform-specific PackageTool (Linux/macOS)
- After completion, signal triggers re-fetch of list

#### **Services Page** (nexis/Pages/Services/)
Simpler template with filtering (dropdown combos):

**UI Layout:**
- Top: Two filter combos ("Running Status", "Startup Status")
- Middle: `QListWidget` with custom `ServiceItem` widgets
- Status label with count

**Data Model:**
```cpp
struct Service {
    QString name;
    QString description;
    bool status;       // Enabled/disabled (startup)
    bool active;       // Running/not running
};
```

**Key pattern:**
- Combos trigger `on_cmbRunningStatus_currentIndexChanged()` → `loadServices()`
- `loadServices()` filters `mServices` in-memory and rebuilds `QListWidget`
- No re-fetching on filter change (data already in memory)

---

### 1.3 Page Registration in app.cpp & app.ui

**Conditional Page Visibility (APT Source Manager / Homebrew example):**

In `app.cpp::init()`:
```cpp
// APT SOURCE MANAGER (only on Linux, or Homebrew on macOS)
if (ToolManager::ins()->checkSourceRepository()) {
    aptSourceManagerPage = new APTSourceManagerPage(mSlidingStacked);
    mListPages.insert(7, aptSourceManagerPage);        // Insert at position 7
    mListSidebarButtons.insert(7, ui->btnAptSourceManager);
} else {
    ui->btnAptSourceManager->hide();   // Hide sidebar button if not available
}

// GNOME SETTINGS (only on Linux with GNOME)
if (ToolManager::ins()->checkGnomeSettings()) {
    gnomeSettingsPage = new GnomeSettingsPage(mSlidingStacked);
    mListPages.insert(settingsIdx, gnomeSettingsPage);
    mListSidebarButtons.insert(settingsIdx, ui->btnGnomeSettings);
} else {
    ui->btnGnomeSettings->hide();
}
```

**Pattern:**
1. Button defined in `app.ui` (all buttons pre-created but hidden if not applicable)
2. Check platform/tool availability via `ToolManager::checkXxx()`
3. If available: instantiate page, insert into lists at correct position
4. If not available: hide the button
5. Wire `on_btnXxx_clicked()` slot to page navigation

**Button placement in app.ui:**
- All buttons are `QPushButton` widgets in sidebar `QVBoxLayout`
- Each button: `checkable=true`, `toolTip="Page Name"`, `iconSize=28x28`
- Icon loaded dynamically in `updateSidebarIcons()` with theme fallback

---

### 1.4 CMakeLists.txt: Adding New Pages

**Structure:**
```cmake
# Step 1: List UI search paths (so AUTOUIC finds .ui files)
set(CMAKE_AUTOUIC_SEARCH_PATHS
  "${GUI_SHARED_DIR}/Pages/Docker"  # Add here
  # ... existing paths
)

# Step 2: Include directory (so #include "docker_page.h" resolves)
target_include_directories(nexis PRIVATE
  "${GUI_SHARED_DIR}/Pages/Docker"  # Add here
  # ... existing paths
)

# Step 3: Source files are auto-globbed (GLOB_RECURSE **.cpp)
# No manual file listing needed
```

**Key point:** CMakeLists handles discovery automatically via `GLOB_RECURSE` and `AUTOUIC_SEARCH_PATHS`. Just create the files in the correct directory and they'll be picked up.

---

## 2. COMMAND EXECUTION & PLATFORM ABSTRACTIONS

### 2.1 CommandUtil API

**Core execution functions:**

```cpp
// Blocking execution, returns stdout (throws on error)
QString CommandUtil::exec(
    const QString &cmd,
    QStringList args = QStringList(),
    QByteArray data = QByteArray(),
    int timeoutMs = 30000
);

// Blocking execution, returns struct with stdout/stderr/exit code
ExecResult CommandUtil::execWithStatus(
    const QString &cmd,
    QStringList args = QStringList(),
    int timeoutMs = 30000
);

// Elevated (sudo) execution
QString CommandUtil::sudoExec(
    const QString &cmd,
    QStringList args = QStringList(),
    QByteArray data = QByteArray()
);

// Check if command is available in PATH
bool CommandUtil::isExecutable(const QString &cmd);
```

**Usage example (from ServiceTool):**
```cpp
QStringList lines = CommandUtil::exec("launchctl", {"list"}).split(QChar('\n'));
// Or with error handling:
ExecResult result = CommandUtil::execWithStatus("docker", {"version"});
if (result.exitCode == 0) { /* success */ }
```

**Key patterns:**
- All calls are blocking (run on worker thread, not main thread)
- Timeout: 30s default, configurable
- Process killed after timeout
- Output trimmed of whitespace
- Errors thrown as `QString` exception on non-zero exit (use try/catch)
- `isExecutable()` checks if command is in PATH (safe way to detect Docker)

### 2.2 Platform-Specific Implementations

**File structure:**
```
shared/nexis-core/Tools/     <- Shared code, structs
  service_tool_shared.cpp
  package_tool_shared.h      <- struct Package, enum PackageTools

macos/nexis-core/Tools/      <- macOS implementations
  service_tool.cpp
  package_tool.cpp
  package_tool.h

linux/nexis-core/Tools/      <- Linux implementations
  service_tool.cpp
  package_tool.cpp
  package_tool.h
```

**Pattern for conditional builds:**
1. Shared header (with ifdef Q_OS_MAC guards) or separate `.h` per platform
2. Platform `.cpp` files in `macos/` and `linux/` directories
3. CMakeLists includes appropriate platform dir first (shadows shared if both exist)

**For Docker:** Can use single implementation since Docker CLI is identical on all platforms. Consider single header + single implementation in shared/.

---

## 3. DOCKER MANAGEMENT: FEATURE SCOPE & CLI COMMANDS

### 3.1 Docker Detection

**Check if Docker is installed:**
```cpp
bool CommandUtil::isExecutable("docker")  // In PATH?
```

**Check Docker daemon status:**
```cpp
// Try: docker version
// Exit code 0 → daemon running
// Exit code != 0 → daemon not running or Docker not installed
ExecResult result = CommandUtil::execWithStatus("docker", {"version"});
bool daemonRunning = (result.exitCode == 0);
```

**Check Docker compose availability:**
```cpp
bool hasCompose = CommandUtil::isExecutable("docker-compose") ||
                  CommandUtil::isExecutable("docker") &&
                  result.output.contains("Compose version");
```

---

### 3.2 Docker CLI Commands for Management

#### **Images Management**
```bash
# List all images (local + dangling)
docker images --all --quiet                    # Just IDs
docker images --all --format "table {{...}}"  # Formatted output

# Get image details (JSON for structured parsing)
docker image inspect <image-id>

# Remove unused images (dangling only, safe)
docker image prune --force

# Remove specific image
docker rmi <image-id>

# Remove image with dependents
docker rmi -f <image-id>

# Get image size
docker images --format "table {{.ID}}\t{{.Size}}"
```

**Output parsing example:**
```bash
$ docker images --all --format "{{.Repository}}|{{.Tag}}|{{.ID}}|{{.Size}}|{{.CreatedAt}}"
ubuntu|latest|sha256:abc123|25.3MB|2025-02-18 10:00:00 +0000 UTC
```

#### **Containers Management**
```bash
# List running containers
docker ps

# List all containers (including stopped)
docker ps --all

# Get container status
docker ps --all --format "{{.ID}}|{{.Status}}|{{.Names}}"

# Get container details
docker inspect <container-id>

# Stop container
docker stop <container-id>

# Start container
docker start <container-id>

# Remove container
docker rm <container-id>

# Remove container (force, even if running)
docker rm -f <container-id>

# Remove stopped containers (safe prune)
docker container prune --force
```

#### **Volumes Management**
```bash
# List all volumes
docker volume ls
docker volume ls --format "table {{.Name}}\t{{.Driver}}\t{{.Mountpoint}}"

# Inspect volume
docker volume inspect <volume-name>

# Remove volume
docker volume rm <volume-name>

# Remove unused volumes (dangling only)
docker volume prune --force

# Get volume size (requires inspect, no direct command)
# Size not directly available; must calculate from mountpoint
```

#### **Docker Compose**
```bash
# If compose v2 (integrated):
docker compose version

# If compose v1 (standalone):
docker-compose version

# List services/containers from compose file
docker compose ps          # compose v2
docker-compose ps          # compose v1

# Prune compose volumes
docker compose down -v     # Remove volumes
```

---

### 3.3 Logical Feature Set for Nexis

**Core features (MVP - Minimum Viable Product):**

1. **Docker Detection**
   - Check if `docker` CLI is in PATH
   - Check if Docker daemon is running (via `docker version`)
   - Conditionally show Docker page only if Docker installed

2. **Images Tab**
   - List all images (repository, tag, size, creation date, used status)
   - Show "Dangling" badge for unused images
   - Search/filter by repository name
   - Action: Remove selected images (with confirmation)
   - Action: Prune dangling images (bulk cleanup)

3. **Containers Tab**
   - List all containers (name, status, creation date, port mapping)
   - Filter by status (Running, Stopped, All)
   - Show last few log lines (if stopped)
   - Actions: Start, Stop, Remove (context menu or buttons)
   - Action: Remove stopped containers (bulk cleanup)

4. **Volumes Tab**
   - List all volumes (name, driver, mountpoint, usage indicator)
   - Filter by usage (used/unused)
   - Action: Remove selected volumes (with warning)
   - Action: Prune unused volumes (bulk cleanup)

5. **System Cleaner Integration** (optional, Phase 2)
   - Add "Docker" category to System Cleaner
   - Checkbox to clean dangling images, volumes, stopped containers
   - Estimate space savings before cleanup

**Advanced features (Phase 2+):**
- Docker Compose file detection and management
- Real-time container stats (CPU, memory) from `docker stats`
- Container logs viewer
- Image build history
- Network introspection
- Container stats history on Resources page

---

### 3.4 Data Structures

**Proposed Docker data types:**

```cpp
// In shared/nexis-core/Tools/docker_tool_shared.h

struct DockerImage {
    QString id;              // sha256:abc123...
    QString repository;      // "ubuntu", "nginx", "myapp"
    QString tag;             // "latest", "1.0", "focal"
    QString size;            // "25.3 MB"
    QString sizeBytes;       // For sorting (QString or qint64)
    QString createdAt;       // "2025-02-18T10:00:00Z"
    bool isDangling;         // true if <none>:<none>
    bool isUsed;             // true if referenced by container
};

struct DockerContainer {
    QString id;              // abc123def456...
    QString names;           // "my-app", "nginx-server"
    QString status;          // "Up 2 days", "Exited (0) 3 hours ago"
    QString statusEnum;      // "running", "exited", "paused"
    QString ports;           // "0.0.0.0:8080->80/tcp"
    QString image;           // "nginx:latest"
    QString createdAt;       // "2025-02-18T10:00:00Z"
};

struct DockerVolume {
    QString name;            // "my-data", "postgres-db"
    QString driver;          // "local"
    QString mountpoint;      // "/var/lib/docker/volumes/my-data/_data"
    qint64 sizeBytes;        // -1 if unknown (requires stat of mountpoint)
    bool isUsed;             // true if referenced by container
};
```

---

## 4. UI DESIGN & USER EXPERIENCE

### 4.1 Layout (Following Uninstaller Pattern)

```
┌─────────────────────────────────────────────┐
│ Docker Management                           │
├─────────────────────────────────────────────┤
│ ☐ Search docker resources... [Filter ▼]    │
├─────────────────────────────────────────────┤
│ [Images] [Containers] [Volumes]  [Settings]│
├─────────────────────────────────────────────┤
│                                             │
│ Images (12)                                 │
│ ┌───────────────────────────────────────┐  │
│ │ ☑ ubuntu:latest         25.3 MB   ✗  │  │
│ │ ☑ nginx:latest          45.2 MB       │  │
│ │ ☐ myapp:1.0             120 MB    [D] │  │
│ │ ☑ postgres:13           200 MB        │  │
│ │ ... (more items)                      │  │
│ └───────────────────────────────────────┘  │
│                                             │
│ ⓘ 3 dangling images taking 45.2 MB         │
│                       [Prune Dangling]      │
│ [Remove Selected] [Refresh]                 │
│                                             │
│ ✓ Docker daemon is running                  │
└─────────────────────────────────────────────┘
```

**Tab structure:**
1. **Images Tab**
   - Tree widget or table: Repository | Tag | Size | Created | Status badge
   - Checkboxes for multi-select
   - Status icons: dangling (grayed), in-use (✓), orphaned (✗)
   - Search filters by name
   - Buttons: "Remove Selected", "Prune Dangling"

2. **Containers Tab**
   - Table: Name | Status | Image | Ports | Created
   - Color-coded status: green (running), gray (exited), yellow (paused)
   - Filter combo: "All / Running / Exited / Paused"
   - Right-click context menu: Start, Stop, Remove, View Logs (future)
   - Buttons: "Remove Stopped", "Refresh"

3. **Volumes Tab**
   - Table: Name | Driver | Mount Path | Size | Used
   - Filter: "All / Used / Unused"
   - Buttons: "Remove Selected", "Prune Unused"
   - Warning: "Removing volumes is permanent"

4. **Status Bar**
   - "Docker daemon: Running" (green) or "Not running" (red)
   - Docker version (if available)
   - Space saved estimate (after cleanup actions)

---

### 4.2 User Interactions & Workflows

**Workflow 1: Clean up dangling images**
1. User opens Docker page
2. Images tab shows list with "Dangling" badges
3. User sees info box: "3 dangling images taking 45.2 MB"
4. Clicks "Prune Dangling"
5. Confirmation dialog: "This cannot be undone"
6. Process runs, list refreshes, shows "Cleanup complete: 45.2 MB freed"

**Workflow 2: Remove specific container**
1. Containers tab, filter set to "All"
2. User right-clicks stopped container → "Remove"
3. Confirmation: "Remove container 'myapp'?"
4. Container removed, list refreshes

**Workflow 3: Check system Docker health**
1. User opens Docker page
2. Status bar shows: "Docker daemon: Running (version 24.0.5)"
3. User can see at a glance how many images/containers/volumes exist
4. "System Cleaner integration" shows option to bulk-clean all unused resources

---

### 4.3 Error Handling & Edge Cases

**Docker not installed:**
- Page shows: "Docker is not installed or not found in PATH"
- Offer link/button to install instructions (external)
- No error logs, graceful message

**Docker daemon not running:**
- Status bar: "Docker daemon: Not running" (red icon)
- List panels show placeholder: "Docker daemon is not running. Start Docker and refresh."
- Refresh button always available

**Permission errors (Linux/macOS):**
- Some operations may require `sudo`
- Use `CommandUtil::sudoExec()` with user consent
- Show message: "This action requires elevated privileges"

**Network connectivity:**
- `docker` commands should be local, no network dependencies
- Image pull/push are out of scope

---

## 5. PLATFORM CONSIDERATIONS

### 5.1 macOS Specifics

**Docker Desktop for Mac:**
- Available via Homebrew: `brew install docker`
- Installs `docker` CLI + Docker daemon in lightweight VM
- Command-line identical to Linux
- Docker socket: `/var/run/docker.sock`

**Detection:**
```cpp
bool hasDocker = CommandUtil::isExecutable("docker");
// If Homebrew page already exists, Docker likely available too
```

**Permissions:**
- Docker CLI usually works for standard user (no sudo needed)
- If permission denied, user added to docker group (or try sudo)

---

### 5.2 Linux Specifics

**Package managers (varies by distro):**
- Ubuntu/Debian: `apt install docker.io`
- Fedora/RHEL: `dnf install docker`
- Arch: `pacman -S docker`
- openSUSE: `zypper install docker`

**Daemon management:**
- Systemd service: `systemctl start docker`
- Socket: `/var/run/docker.sock`
- Group membership: User must be in `docker` group or use `sudo docker`

**Detection:**
```cpp
bool hasDocker = CommandUtil::isExecutable("docker");
bool daemonRunning = CommandUtil::execWithStatus("docker", {"version"}).exitCode == 0;
```

**Permissions:**
- If user not in docker group: commands fail with "permission denied"
- Solution: Add user to docker group (requires logout/login) or use sudo
- Consider prompting: "Docker requires elevated privileges on this system"

---

### 5.3 Cross-Platform Compatibility

**Command output formats:**
- `docker images` output is consistent across platforms
- JSON output (`--format`) is consistent
- Newline handling: Use `QString::split(QChar('\n'))` and `trimmed()` to handle both Unix and Windows line endings (though Docker is rare on Windows desktop)

**Path handling:**
- Mount paths on macOS: `/var/lib/docker/volumes/...` (inside VM)
- Mount paths on Linux: `/var/lib/docker/volumes/...` or custom
- Use forward slashes in code (Qt handles conversion)

**No Windows support:**
- Docker on Windows requires WSL2 (containers run in Linux VM)
- Docker Desktop Windows exists but Nexis targets Linux/macOS
- Can skip Windows support initially; add if demand arises

---

## 6. SECURITY IMPLICATIONS

### 6.1 Privilege Escalation Risks

**Risk:** Unprivileged user trying to remove images/containers owned by another user or root

**Mitigation:**
- Always show docker daemon status upfront
- If commands fail with permission error, show helpful message (not silent failure)
- Consider `sudo` wrapper for privileged operations (with user confirmation)
- Use `CommandUtil::sudoExec()` for operations that likely need elevation

**Example:**
```cpp
bool success = false;
try {
    success = (CommandUtil::execWithStatus("docker", {"rmi", imageId}).exitCode == 0);
} catch(const QString &err) {
    if (err.contains("permission denied")) {
        // Ask user via dialog: "Requires elevated privileges. Retry with sudo?"
        // If yes: CommandUtil::sudoExec("docker", {"rmi", imageId});
    }
}
```

---

### 6.2 Data Exposure

**Risk:** User examining container filesystem via volume mount paths

**Mitigation:**
- Don't display volume contents, only mountpoint paths
- Don't add "open in file manager" action for volumes (too dangerous)
- Warn users: "Be careful removing volumes — data is permanently lost"

---

### 6.3 Resource Exhaustion

**Risk:** Removing many large images/volumes in rapid succession could cause system hang

**Mitigation:**
- Run operations asynchronously (QtConcurrent)
- Show progress bar or busy indicator
- Allow user to cancel ongoing operations
- Batch removals with small delays between commands (e.g., 100ms between docker rmi calls)

---

## 7. IMPLEMENTATION ROADMAP

### Phase 1: MVP (Docker detection + Images tab)
1. Create `docker_tool.h` and `docker_tool.cpp` (shared core)
2. Implement Docker detection (`isDockerAvailable()`, `isDaemonRunning()`)
3. Create `DockerImage` struct
4. Implement `getDockerImages()` → parses `docker images --all --format ...`
5. Create `docker_page.h/cpp` with Images tab only
6. Create `docker_page.ui` with tree widget, search, buttons
7. Wire into app.cpp/app.ui (sidebar button, page registration)
8. Test on macOS + Linux

**Deliverables:**
- Docker page visible only if Docker installed
- Images listed with name, tag, size, creation date
- Dangling images highlighted
- "Prune Dangling" action works
- "Remove Selected" action works
- Refresh button works

---

### Phase 2: Containers & Volumes tabs
1. Implement `DockerContainer` and `DockerVolume` structs
2. Implement `getDockerContainers()`, `getDockerVolumes()`
3. Add Container and Volume tabs to docker_page.ui
4. Implement filtering logic (status filters, used/unused)
5. Wire up Start, Stop, Remove actions
6. Test on both platforms

---

### Phase 3: System Cleaner integration
1. Add "Docker" category to System Cleaner
2. Checkbox options: clean dangling images, volumes, stopped containers
3. Estimate space savings
4. Implement cleanup via docker prune commands
5. Show cleaned-up bytes after completion

---

### Phase 4: Advanced features (future)
- Container logs viewer
- Docker Compose support
- Real-time container stats on Resources page
- Historical trends
- Network inspection

---

## 8. REFERENCE ARCHITECTURE OVERVIEW

```
nexis/Pages/Docker/
├── docker_page.h           # Q_OBJECT, loadImages(), loadContainers(), etc.
├── docker_page.cpp         # UI updates, filtering, action handling
└── docker_page.ui          # QTabWidget(Images/Containers/Volumes), buttons

nexis-core/Tools/
├── docker_tool.h           # Shared structs, class definition
├── docker_tool_shared.cpp  # isDockerAvailable(), isDockerDaemonRunning()
└── (Optional platform dirs if needed)
    └── docker_tool.cpp     # getDockerImages(), getDockerContainers(), etc.

Managers/
└── tool_manager.h          # Add docker_tool getter methods

Integrations:
├── app.cpp                 # Register page if Docker available
├── app.ui                  # Add btnDocker button to sidebar
└── system_cleaner_page     # Optional: Docker cleanup category
```

---

## 9. KEY FINDINGS & RECOMMENDATIONS

### 9.1 Strengths of This Feature

1. **Clear niche fit:** Aligns with "developer-focused cleaning" direction (containers waste significant storage)
2. **Cross-platform:** Docker CLI identical on macOS/Linux, single codebase
3. **Proven pattern:** Follows established Uninstaller + Services templates
4. **Low complexity (MVP):** Docker CLI provides structured output, easy to parse
5. **User demand:** Growing containerization on desktops; no competitor offers this in a GUI optimizer

### 9.2 Implementation Challenges

1. **Permissions:** Linux users may need docker group membership or sudo (UI/UX challenge)
2. **Async operations:** Bulk removal of many large images takes time (need progress feedback)
3. **Error recovery:** If user cancels mid-operation, state may be inconsistent
4. **Docker Compose:** Optional, but increasingly common; adds scope if included

### 9.3 Recommended Phase 1 Scope

**Keep it simple for MVP:**
- Images tab only (Containers/Volumes in Phase 2)
- Prune dangling images + remove selected
- No bulk recursive operations
- No Docker Compose support
- No real-time stats

**This keeps Phase 1 achievable in 1-2 development sessions.**

---

## 10. RELATED ISSUES & UPSTREAM REFERENCES

- **Stacer issue #454:** Original feature request (archived)
- **Market Research (section 10.2):** Docker management rated "Medium" priority in competitive landscape
- **FR-03 (System Cleaner expansion):** Already handles Electron/dev tool caches; Docker extends this pattern
- **FR-16 (Scheduled cleaning):** Docker could be integrated into scheduled cleanup tasks (Phase 3+)

---

## 11. CONCLUSION

Docker image/volume management is a well-scoped, implementable feature that differentiates Nexis in an underserved market niche. The MVP (Docker detection + Images tab) can be completed in Phase 1 by following established patterns in the codebase (Uninstaller, Services). Subsequent phases (Containers, Volumes, System Cleaner integration) expand functionality without major architectural changes.

The feature leverages existing infrastructure (CommandUtil, QtConcurrent, ToolManager singleton) and fits naturally into the page registration system already handling conditional features (APT, Homebrew, GNOME Settings).

**Recommendation:** Proceed to Phase 2 (Implementation planning) once this research is approved.
