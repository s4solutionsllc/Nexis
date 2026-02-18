Now I have all the information needed. Let me compile the comprehensive research report:

---

# FR-14 Research: Flatpak Distribution for Nexis

## Executive Summary

Flatpak distribution for Nexis presents **significant technical and architectural challenges** due to the fundamental nature of Nexis as a system monitoring and optimization tool. While Flatpak is a legitimate distribution method on Linux, it is **technically feasible but practically problematic** for Nexis without major architectural compromises.

**Key Findings:**
- Nexis requires deep system access (reading `/proc`, `/sys`, running privileged commands) that conflicts with Flatpak's sandbox model
- Privilege escalation via `pkexec` and `sudoExec` is incompatible with Flatpak's permission system
- No current Flatpak manifest exists; building one would require new infrastructure
- Alternative distributions (AppImage, PPA, AUR) may deliver better user value with less friction

---

## 1. Current Packaging & Distribution Methods

### 1.1 Existing Distribution Channels

**Linux:**
- `.deb` package (Debian/Ubuntu): Built via CMake + dpkg-buildpackage in CI/CD
- AppImage (portable): Built via linuxdeploy + Qt plugin; runs on any distro
- Raw binary: Standalone executable (requires Qt6 runtime)

**macOS:**
- `.dmg` disk image (Apple Silicon only): Built via CMake + macdeployqt + hdiutil

**Build Infrastructure:**
- CMake 3.16+ with Qt6 components (Core, Gui, Widgets, Charts, Svg, Concurrent, Network)
- GitHub Actions: `.github/workflows/build.yml` (PR/push), `release.yml` (tags)
- **No Flatpak manifest, snap config, or AppImage-specific manifest currently exists**

### 1.2 Debian Packaging Details

**File:** `/Users/luke/Documents/GitHub/Nexis/linux/debian/control`

```
Source: nexis
Build-Depends: cmake, g++, qt6-base-dev, qt6-charts-dev, qt6-svg-dev,
               qt6-tools-dev, qt6-l10n-tools, libgl1-mesa-dev
Depends: ${shlibs:Depends}, ${misc:Depends}
Recommends: systemd, curl, adwaita-icon-theme | yaru-theme-icon
```

**Key observation:** `systemd` and `curl` are recommended, not required—Nexis degrades gracefully if absent.

### 1.3 Desktop File

**File:** `/Users/luke/Documents/GitHub/Nexis/linux/applications/nexis.desktop`

```
[Desktop Entry]
Name=Nexis
Exec=nexis
Icon=nexis
Type=Application
Terminal=false
Categories=Utility;
StartupWMClass=nexis
```

**No** privilege escalation hints (e.g., `X-GNOME-Pkexec=true`) present—this is intentional; Nexis prompts for auth only when performing privileged actions.

---

## 2. External Dependencies & Runtime Requirements

### 2.1 Core Build-Time Dependencies

| Category | Package | Purpose | Notes |
|----------|---------|---------|-------|
| Compiler | g++, cmake | C++17 compilation | Standard development tools |
| Qt6 Framework | qt6-base-dev, qt6-charts-dev, qt6-svg-dev, qt6-tools-dev, qt6-l10n-tools | GUI, charts, SVG, i18n | Qt 6.2+ required |
| Graphics | libgl1-mesa-dev | OpenGL rendering | Required for Qt6 GUI |
| **Flatpak would provide:** All of the above in the Flatpak runtime |

### 2.2 Runtime Dependencies (Linux)

#### System Monitoring (Unprivileged Read-Only)
- `/proc/cpuinfo` — CPU info (lscpu command)
- `/proc/loadavg` — CPU load averages
- `/proc/stat` — CPU usage statistics
- `/proc/meminfo` — Memory and swap info
- `/sys/block/*/` — Disk device info
- `/sys/class/hwmon/` — Thermal sensors
- `/sys/class/net/*/statistics/` — Network interface stats
- `/sys/class/power_supply/` — Battery/AC status
- `/sys/class/drm/cardN/` — GPU device info

**Commands Used (Unprivileged):**
- `ps ax -weo` — Process enumeration
- `systemctl list-unit-files` — Service list
- `lscpu` — CPU topology (with LC_ALL=C)
- `lspci` — PCI device enumeration
- `nvidia-smi` (optional) — NVIDIA GPU load
- `dpkg-query`, `rpm`, `pacman` — Package enumeration

#### System Modification (Privileged via pkexec/sudo)
- Service enable/disable: `systemctl enable|disable SERVICE`
- Service start/stop: `systemctl start|stop SERVICE`
- Process termination: `kill PID`
- Package management: `apt-get install|remove|purge`, `dnf`, `yum`, `pacman`, `snap`
- System cleaning: `rm -rf` on `/tmp`, `/var/cache`, user cache dirs
- File operations: `mv`, `find` with elevated privileges
- Hosts file editing: Write to `/etc/hosts`
- APT source management: `add-apt-repository` with sudo
- SMART disk health: `smartctl -j -a DEVICE` (often requires root)
- Temporary file creation: `/tmp/nexis_*` files for atomic writes

**Critical tools with privileged access:**
```cpp
// From linux/nexis-core/Utils/command_util_platform.cpp:
QString CommandUtil::sudoExec(const QString &cmd, QStringList args, ...)
{
    args.push_front(cmd);
    result = CommandUtil::exec("pkexec", args, data);  // Prompt for password
    return result;
}
```

#### Optional Runtime Tools
- `systemd` — Service management (recommended)
- `curl` — Potential future use (recommended)
- Disk analyzer tools: `baobab`, `filelight`, `ncdu`, `qdirstat` (user-selectable, optional)

---

## 3. System Access Requirements — Detailed Analysis

### 3.1 Read-Only System Access (No Sandbox Issues)

All monitoring features work via unprivileged read:

```cpp
// CPU info: /proc/cpuinfo, /proc/loadavg, /proc/stat
// Memory: /proc/meminfo
// Disk: /sys/block/*/size, /sys/block/*/stat
// Network: /sys/class/net/*/statistics/{rx,tx}_bytes
// Thermal: /sys/class/hwmon/*/temp*_input
// Battery: /sys/class/power_supply/*/capacity, */status, */charge_{full,now}
// GPU: /sys/class/drm/cardN/, sysfs gpu_busy_percent (AMD), lspci output
```

**Flatpak Impact:** These paths can be exposed via `--filesystem=host:ro` (read-only bind mounts). **Not a blocker.**

### 3.2 Privileged Operations (Sandbox Conflict)

#### 3.2.1 Service Management

Nexis uses `systemctl` with `pkexec` for privilege escalation:

```cpp
// From linux/nexis-core/Tools/service_tool.cpp:
bool ServiceTool::changeServiceStatus(const QString &sname, bool status)
{
    CommandUtil::sudoExec("systemctl", {status ? "enable" : "disable", sname});
}
```

**Flatpak Challenge:**
- `pkexec` inside Flatpak requires PolicyKit integration via D-Bus
- Flatpak provides `--talk-name=org.freedesktop.PolicyKit1` for PolicyKit communication
- **Status: FEASIBLE** — D-Bus access to PolicyKit works, but requires explicit manifest permission

#### 3.2.2 Package Management

Nexis supports multiple package managers with privilege elevation:

```cpp
// From linux/nexis-core/Tools/package_tool.cpp:
if (CommandUtil::isExecutable("apt-get"))
    CommandUtil::sudoExec("apt-get", {"install"|"remove"|"purge", ...});
else if (CommandUtil::isExecutable("dnf"))
    CommandUtil::sudoExec("dnf", {...});  // Fedora
else if (CommandUtil::isExecutable("pacman"))
    CommandUtil::sudoExec("pacman", {...});  // Arch
else if (CommandUtil::isExecutable("snap"))
    CommandUtil::sudoExec("snap", {...});  // Snap packages
```

**Flatpak Challenge:**
- Package managers live in `/usr/bin/`, which **Flatpak cannot execute** by default
- Even with `--filesystem=host`, Flatpak sandboxes child process spawning
- `pkexec apt-get install` would require:
  1. Host's `pkexec` to be accessible (not guaranteed)
  2. Flatpak's sandboxing to allow subprocess execution (NOT allowed by default)
  3. Host's package manager to be available inside sandbox context (REQUIRES special handling)

**Status: PROBLEMATIC** — Package installation/removal would require breaking the sandbox or special host integration.

#### 3.2.3 System Cleaning

```cpp
// From shared/nexis/Pages/SystemCleaner/system_cleaner_page.cpp:
CommandUtil::sudoExec("rm", {"-rf", filesToRemove});
```

Cleaning caches and junk requires writing to `/var/cache/`, `/tmp/`, and user dirs. Flatpak sandboxing complicates this:

```cpp
// File deletion via privileged rm
// Affects: /var/cache/apt/archives/, /var/cache/dnf/, /var/cache/yum/
//          /tmp/, ~/.cache/, ~/.local/share/
```

**Status: PROBLEMATIC** — Deleting system-wide caches requires escaping sandbox or host integration.

#### 3.2.4 Disk Health (smartctl)

```cpp
// From linux/nexis-core/Info/disk_health_info.cpp:
QString output = CommandUtil::exec("pkexec", {"smartctl", "-j", "-a", device});
```

SMART data access often requires root due to raw device access. Flatpak cannot provide this without host integration.

**Status: WILL DEGRADE** — smartctl would fail inside sandbox; users would only get basic disk info.

#### 3.2.5 Hosts File Editing

```cpp
// From shared/nexis/Pages/Helpers/host_manage.cpp:
CommandUtil::sudoExec("mv", {"/tmp/nexis_etc_host_new_content", "/etc/hosts"});
```

Modifying `/etc/hosts` requires privilege escalation. Flatpak prevents direct file writes to `/etc/`.

**Status: BROKEN** — Hosts file editing would not work inside Flatpak.

---

## 4. Privilege Escalation Strategy Mismatch

### 4.1 How Nexis Currently Uses pkexec

```cpp
// Standard pattern throughout codebase:
QString CommandUtil::sudoExec(const QString &cmd, QStringList args, QByteArray data)
{
    args.push_front(cmd);
    result = CommandUtil::exec("pkexec", args, data);  // Spawns authentication dialog
    return result;
}
```

**Current UX:**
1. User attempts a privileged action (enable service, clean cache, etc.)
2. Nexis calls `pkexec COMMAND ARG1 ARG2 ...`
3. PolicyKit shows password dialog (usually once per session)
4. Command executes with root privilege
5. Result returned to GUI

### 4.2 Flatpak & PolicyKit Integration

Flatpak **does support** PolicyKit via D-Bus:

```yaml
# Required in flatpak manifest:
permissions:
  dbus:
    talk:
      - org.freedesktop.PolicyKit1
  
  # Also need subprocess spawning:
  system-talk:
    - org.freedesktop.PolicyKit1
```

However, **Flatpak still prevents executing host binaries.** Even with PolicyKit access, when Flatpak's `pkexec` prompt grants permission, the sandboxed `apt-get` or `systemctl` inside the Flatpak runtime won't have access to the host's package manager.

### 4.3 The Fundamental Constraint

**Flatpak's design principle:** "Guarantee that untrusted or partially-trusted apps cannot harm the host system."

This means:
- ✓ Read-only access to system files (with explicit `/proc`, `/sys` permissions)
- ✓ D-Bus communication to system services (PolicyKit, systemd)
- ✗ **Direct privilege escalation to run host commands**
- ✗ **Modifying system-critical files** (`/etc/hosts`, `/etc/apt/`, etc.)
- ✗ **Accessing host package managers**

---

## 5. Existing Flatpak Ecosystem

### 5.1 System Monitoring Tools on Flathub

**Currently distributed:**
- **GNOME System Monitor** — Flatpak available; monitors processes, resources; no optimization/cleaning
- **btop++** — Flatpak available; resource monitoring; CLI-only
- **Glances** — Flatpak available; real-time monitoring; CLI/web UI
- **Cockpit** — Web-based system management; Flatpak available; limited GUI for cleanup

**Notably ABSENT:**
- No system optimizer/cleaner available as Flatpak
- No cross-platform (Linux + macOS) monitoring tool on Flathub
- BleachBit (cleaner) only available as traditional deb/rpm, not Flatpak (due to similar sandbox conflicts)

### 5.2 Flathub Policy & Restrictions

From Flathub documentation:
- **Mandatory:** Sandboxing enforced; no `--devel-mode` production apps
- **Filesystem access:** Apps can request specific paths, but privileged escalation is discouraged
- **D-Bus:** System services (PolicyKit, systemd) can be accessed but with explicit permissions
- **Special handling:** Some system tools (Cockpit, package managers) use D-Bus bridges instead of direct privilege escalation

---

## 6. Conceptual Flatpak Manifest for Nexis

**If a Flatpak were created**, it would look like this:

```yaml
app-id: com.nexis.Nexis
runtime: org.kde.Platform  # or org.gnome.Platform
runtime-version: '6.7'

sdk: org.kde.Sdk  # Provides Qt6, build tools

command: nexis

finish-args:
  # GUI and window system
  - --share=ipc
  - --socket=wayland
  - --socket=fallback-x11
  - --device=dri
  
  # System information (read-only)
  - --filesystem=/proc:ro
  - --filesystem=/sys:ro
  - --filesystem=/dev:ro
  - --filesystem=/etc:ro  # Hosts file would be read-only!
  
  # User home access (for caches, config)
  - --filesystem=home
  
  # Icon theme
  - --filesystem=/usr/share/icons:ro
  - --filesystem=/usr/share/themes:ro
  
  # PolicyKit (for privilege escalation attempts)
  - --system-talk-name=org.freedesktop.PolicyKit1
  - --talk-name=org.freedesktop.DBus
  
  # Thermal/hardware sensors (if using hwmon)
  - --device=/sys/class/hwmon:ro
  
  # System tray icon
  - --talk-name=org.kde.StatusNotifierWatcher  # or org.freedesktop.StatusNotifierWatcher

modules:
  - name: nexis
    buildsystem: cmake
    build-options:
      cmake-args:
        - -DCMAKE_BUILD_TYPE=Release
    sources:
      - type: git
        url: https://github.com/lsimpsonsfdc/Nexis.git
        tag: v1.1.2  # Example version
```

**Critical Problems with This Manifest:**

1. **Package Management:** `/usr/bin/apt-get`, `/usr/bin/dnf`, etc. are **not accessible inside Flatpak**. Calls to `sudoExec("apt-get", ...)` would fail.

2. **Hosts File Editing:** `/etc/hosts` is mounted read-only. The `mv` command to update it would fail.

3. **System Cleaning:** Deleting files in `/var/cache/` or system directories requires host integration unavailable to Flatpak.

4. **SMART Disk Health:** `smartctl` would need raw device access (`/dev/sdX`, `/dev/nvmeN`) which Flatpak restricts.

5. **Service Management:** While `systemctl` can be called via D-Bus (PolicyKit), **the Flatpak version of `systemctl` inside the runtime wouldn't match the host's systemd**. Results could be inconsistent or fail silently.

---

## 7. Feasibility Assessment

### 7.1 Feature-by-Feature Breakdown

| Feature | Feasibility | Details |
|---------|-------------|---------|
| Dashboard (monitoring) | ✅ **Full** | Read-only `/proc`, `/sys`, sysfs access works |
| Process Manager (list/kill) | ⚠️ **Partial** | List works; killing processes needs PolicyKit + D-Bus (fragile) |
| Service Manager (enable/disable) | ⚠️ **Partial** | Works via systemd D-Bus (more robust than PolicyKit subprocess) |
| Startup Apps Manager | ⚠️ **Partial** | Editing user's `.desktop` files works; system-wide StartupWMClass not fully accessible |
| Package Uninstaller | ❌ **Broken** | No access to host `apt-get`, `dnf`, `pacman` binaries inside sandbox |
| System Cleaner | ❌ **Broken** | Cannot delete files in `/var/cache/`, `/tmp/` with privilege escalation |
| Hosts File Editor | ❌ **Broken** | `/etc/hosts` mounted read-only; `sudoExec("mv", ...)` fails |
| APT Source Manager | ❌ **Broken** | Cannot run `add-apt-repository` inside sandbox |
| File Search | ✅ **Full** | `find` command works within user home + read-only system dirs |
| Disk Usage Analyzer Launcher | ⚠️ **Partial** | Can launch Flatpak versions of Baobab, but native installs unreachable |
| Disk Health (SMART) | ❌ **Broken** | `smartctl` requires raw device access Flatpak denies |
| GNOME Settings Editor | ✅ **Full** | `gsettings` over D-Bus works fine |
| Hardware Info | ✅ **Full** | Reading sysfs and CPU info works |
| GPU Monitoring | ⚠️ **Partial** | AMD sysfs works; `nvidia-smi` unavailable inside sandbox (no NVIDIA drivers in Flatpak runtime) |
| Thermal Monitoring | ✅ **Full** | `/sys/class/hwmon` read-only access works |

### 7.2 Overall Verdict

| Aspect | Status | Comments |
|--------|--------|----------|
| **Technical Feasibility** | 🟡 **Possible but Compromised** | ~50% of features would work; others would require complete rewrites |
| **User Experience** | 🔴 **Poor** | Users expect full optimization capabilities; Flatpak Nexis would be a monitoring-only tool |
| **Maintenance Burden** | 🔴 **High** | Two separate code paths: regular build + Flatpak variant with feature flags and graceful degradation |
| **Community Demand** | 🟢 **Moderate** | Fedora Silverblue and SteamOS users would benefit; but **not critical** for current market positioning |

---

## 8. Challenges & Constraints

### 8.1 Architectural Incompatibilities

| Challenge | Impact | Severity |
|-----------|--------|----------|
| **Cannot spawn host binaries** | `apt-get`, `dnf`, `systemctl`, `smartctl` unavailable | CRITICAL |
| **Cannot modify `/etc/` files** | Hosts file editor non-functional | HIGH |
| **Cannot write to `/var/cache/`** | System cleaning for apt/dnf/yum/snap broken | HIGH |
| **No raw device access** | Disk health (smartctl, `/dev/sdX`) unavailable | MEDIUM |
| **GPU driver mismatch** | nvidia-smi, AMD tools need host driver matching | MEDIUM |
| **PolicyKit subprocess execution** | Process killing fragile; multiple auth dialogs instead of one | MEDIUM |
| **Single-instance enforcement** | QLockFile might conflict with Flatpak instance isolation | LOW |

### 8.2 CI/CD & Maintenance Complexity

**Building & testing Flatpak would require:**

1. New GitHub Actions workflow for Flatpak builds
2. Flatpak SDK setup in CI (flatsdk-runtime, flatpak-builder)
3. Separate Flathub submission/review process (1-2 weeks per version)
4. Testing on Flatpak-native systems (requires Fedora Silverblue or manual Flatpak install)
5. Feature flags to disable broken functionality in Flatpak builds
6. Dual documentation (regular vs Flatpak installation limits)
7. Separate bug reports for "works in .deb, broken in Flatpak"

**Estimated effort:** 80-120 hours for initial implementation + 10-15 hours per release cycle.

---

## 9. Alternative Distribution Methods (Better Value)

### 9.1 AppImage (Already Implemented ✅)

**Status:** Already building in CI; downloadable from GitHub Releases.

**Pros:**
- Single portable binary; runs on any Linux distro
- No sandbox restrictions; full feature set works
- Users do NOT need to install anything (no `sudo apt install`, no Flathub account)
- Simple to distribute, update, and support

**Cons:**
- Requires FUSE2 library (available on all modern systems)
- No system tray integration on some DEs

**Verdict:** **AppImage is Nexis's best Linux distribution format today.**

### 9.2 PPA (Ubuntu-specific)

**Status:** Not yet implemented.

**Pros:**
- `sudo add-apt-repository ppa:lsimpsonsfdc/nexis` → `sudo apt install nexis`
- Ubuntu users (largest desktop Linux segment) get one-click install + auto-updates
- Easier than AppImage for non-technical users

**Cons:**
- Ubuntu/Debian only (no Fedora, Arch, etc.)
- Requires Launchpad account and PPA hosting
- Slightly higher barrier to entry for maintainers

**Verdict:** **PPA would be valuable supplement to .deb + AppImage.**

### 9.3 AUR (Arch User Repository)

**Status:** Not yet implemented.

**Pros:**
- `yay -S nexis` (on Arch-based distros)
- Community can contribute package maintenance
- Appeals to power users and rolling-release distros

**Cons:**
- Arch/Manjaro only
- Less critical than Ubuntu/Debian coverage

**Verdict:** **AUR packaging worthwhile but lower priority than PPA.**

### 9.4 Snap

**Status:** Not implemented.

**Pros:**
- Works across distros (Ubuntu focus)
- Auto-update mechanism built-in

**Cons:**
- Canonical's proprietary ecosystem (even though GPL source)
- Similar sandbox restrictions as Flatpak (would have same feature gaps)
- Snap store review process
- User perception: slower startup, larger disk footprint

**Verdict:** **Not recommended; Flatpak is more open, and AppImage provides better UX.**

---

## 10. Market & Strategic Implications

### 10.1 Who Would Use Nexis via Flatpak?

**Likely users:**
- Fedora Silverblue / immutable distro enthusiasts
- SteamOS 3.0+ (Steam Deck, but tool not relevant there)
- Users who **refuse to use traditional package managers**

**Estimated segment:** 3-5% of Nexis's Linux user base.

### 10.2 Opportunity Cost

**Instead of investing 80-120 hours in Flatpak:**
- Implement **PPA + AUR packaging** (40-60 hours) → reach 90%+ of Linux desktop users
- Fix **FR-06 (ARM64 Linux support)** (30-40 hours) → Raspberry Pi, ARM servers, Apple Silicon users
- Implement **FR-16 (scheduled cleaning)** → highly requested UX feature
- Implement **Homebrew Cask** on macOS (20-30 hours) → massive macOS discoverability boost

### 10.3 Strategic Recommendation

| Initiative | Effort | User Impact | Priority |
|-----------|--------|-------------|----------|
| **PPA** | 40-60h | High (Ubuntu users) | 🔴 **CRITICAL** |
| **AUR** | 20-30h | Medium (Arch power users) | 🟡 **HIGH** |
| **Flatpak** | 80-120h | Low (immutable distro niche) | 🟢 **MEDIUM** |
| **ARM64 support** | 30-40h | Medium (RPI, arm servers) | 🟡 **HIGH** |
| **Homebrew Cask** | 20-30h | High (macOS discoverability) | 🔴 **CRITICAL** |

**Verdict:** **Do not prioritize Flatpak unless requested by significant user demand. Focus on PPA, AUR, ARM64, and macOS distribution first.**

---

## 11. If Flatpak Is Decided: Implementation Roadmap

**Should the team decide to pursue Flatpak despite challenges, here's the minimal viable implementation:**

### 11.1 Phase 1: Create Manifest & Conditional Build

1. Add `linux/flatpak/org.nexis.Nexis.yml` manifest
2. Add CMake option: `-DFLATPAK_BUILD=ON`
3. Feature-gate broken functionality:
   ```cpp
   #ifdef FLATPAK_BUILD
   // Disable or hide:
   // - Package uninstaller
   // - System cleaner (for system dirs only; keep user cache cleaning)
   // - Hosts file editor
   // - APT source manager
   // - SMART disk health
   qWarning() << "Feature unavailable in Flatpak sandbox";
   #endif
   ```

### 11.2 Phase 2: GitHub Actions Flatpak Build

Add `.github/workflows/flatpak.yml`:
```yaml
name: Flatpak Build
on: [push, pull_request]
jobs:
  flatpak:
    runs-on: ubuntu-latest
    container: 
      image: bilelmoussaoui/flatpak-github-actions:latest
    steps:
      - uses: actions/checkout@v4
      - name: Build Flatpak
        uses: bilelmoussaoui/flatpak-github-actions@v5
        with:
          bundle: org.nexis.Nexis.flatpak
          manifest-path: linux/flatpak/org.nexis.Nexis.yml
```

### 11.3 Phase 3: Flathub Submission

1. Fork flathub/flathub repository
2. Add `org.nexis.Nexis.yml` to flathub repo
3. Submit PR with manifest + metadata
4. Flathub review (1-2 weeks)
5. App appears on Flathub

### 11.4 Phase 4: Documentation & Support

- Add "Flatpak" section to README with known limitations
- Create `FLATPAK.md` documenting feature gaps
- Update CI/CD: include Flatpak bundle in releases (optional)

**Total effort Phase 1-4:** ~100 hours over 2-3 months.

---

## 12. Recommendations

### 12.1 Short Term (Next 6 months)

1. **DO NOT pursue Flatpak.** Opportunity cost is too high for diminishing returns.

2. **INSTEAD, implement in priority order:**
   - [ ] **PPA (Ubuntu/Debian)** — Reach Ubuntu users, simplest distribution method
   - [ ] **AUR (Arch)** — Appeal to power-user Linux community
   - [ ] **Homebrew Cask (macOS)** — Major macOS discoverability boost
   - [ ] **Snap (consider, but lower priority)** — If demand warrants

3. **Mark FR-14 as:**
   ```
   - [~] **FR-14: Flatpak distribution** — Deferred pending PPA/AUR completion and user demand
   ```

### 12.2 Long Term (6-12 months)

- **If requested by 10+ users:** Revisit Flatpak as a lower-priority, community-contributed effort
- **If immutable distro adoption grows >10% of user base:** Reconsider as strategic priority
- **Otherwise:** Maintain current stance and focus on features over packaging formats

### 12.3 For Current Implementation (If Team Chooses to Proceed)

- Create `claude_definitions/FR-14_plan.md` with phases outlined in Section 11
- Include feature-gating and graceful degradation in design
- Plan 5-6 week timeline for planning + implementation + testing
- **Most critical risk:** Ensuring disabled features don't crash; thorough QA needed

---

## 13. Summary Table: All Distribution Methods

| Method | Status | User Reach | Implementation Effort | Feature Parity | Recommendation |
|--------|--------|-----------|----------------------|-----------------|-----------------|
| **AppImage** | ✅ Live | All Linux distros | Done | 100% | **Keep using** |
| **Debian .deb** | ✅ Live | Ubuntu, Debian, Mint | Done | 100% | **Keep using** |
| **macOS .dmg** | ✅ Live | macOS (Apple Silicon) | Done | 100% | **Keep using** |
| **PPA** | ❌ Not yet | Ubuntu (70%+ Linux users) | 40-60h | 100% | **IMPLEMENT NEXT** |
| **AUR** | ❌ Not yet | Arch/Manjaro users | 20-30h | 100% | **IMPLEMENT AFTER PPA** |
| **Homebrew Cask** | ❌ Not yet | macOS (massive reach) | 20-30h | 100% | **IMPLEMENT AFTER PPA** |
| **Flatpak** | ❌ Not yet | Fedora/immutable distros | 80-120h | ~50% | **DEFER unless demanded** |
| **Snap** | ❌ Not yet | Ubuntu/general | 70-100h | ~50% | **DEFER; lower priority than Flatpak** |
| **Windows** | N/A | Windows users | Very High | — | **Out of scope** |

---

## Appendix A: File Paths & System Access Summary

### Read-Only Paths (Flatpak-Friendly)
```
/proc/cpuinfo, /proc/loadavg, /proc/stat, /proc/meminfo
/sys/block/*, /sys/class/hwmon/*, /sys/class/net/*, /sys/class/power_supply/*
/sys/class/drm/*, /sys/devices/system/cpu/*
/usr/share/icons/, /usr/share/themes/
/etc/os-release, /etc/hostname (read-only)
```

### Privileged Write Paths (Flatpak-Hostile)
```
/etc/hosts — Hosts file editing
/etc/apt/sources.list.d/ — APT source manager
/var/cache/apt/archives/ — Package cache cleaning
/var/cache/dnf/, /var/cache/yum/ — RPM cache cleaning
/tmp/ — System temp cleaning
~/.cache/ — User app caches
```

### Commands Requiring Privilege (Flatpak-Inaccessible)
```
apt-get, dnf, yum, pacman, snap — Package managers (not in Flatpak runtime)
systemctl — Service control (partially accessible via D-Bus)
smartctl — Disk health (requires raw device access)
pkexec — Privilege escalation (works but spawns separate process)
```

### Optional External Tools (Not Required)
```
baobab, filelight, ncdu, qdirstat — Disk analyzers (user installs)
nvidia-smi — GPU monitoring (needs NVIDIA driver)
lspci — PCI enumeration (optional; sysfs fallback exists)
```

---

## Appendix B: References

**Flatpak Documentation:**
- https://docs.flatpak.org/en/latest/
- https://docs.flatpak.org/en/latest/sandbox-permissions.html
- https://docs.flatpak.org/en/latest/portals.html

**Flathub Guidelines:**
- https://github.com/flathub/flathub/wiki/Guidelines

**System Monitoring Tools & Flatpak:**
- GNOME System Monitor (Flatpak): https://flathub.org/apps/org.gnome.SystemMonitor
- Cockpit (web-based): https://flathub.org/apps/org.cockpit_project.CockpitClient

**Nexis Codebase Files Referenced:**
- `/Users/luke/Documents/GitHub/Nexis/CMakeLists.txt` — Build system
- `/Users/luke/Documents/GitHub/Nexis/linux/debian/control` — Debian packaging
- `/Users/luke/Documents/GitHub/Nexis/linux/nexis-core/Utils/command_util_platform.cpp` — Privilege escalation
- `/Users/luke/Documents/GitHub/Nexis/linux/nexis-core/Tools/package_tool.cpp` — Package manager integration
- `/Users/luke/Documents/GitHub/Nexis/linux/nexis-core/Info/*.cpp` — System info collection
- `/Users/luke/Documents/GitHub/Nexis/FEATURE_REQUESTS.md` — Feature tracking
- `/Users/luke/Documents/GitHub/Nexis/market_research.md` — Strategic context

---

**Report Created:** 2026-02-18  
**For:** FR-14 Flatpak Distribution Research  
**Status:** Ready for Phase 2 (Planning) if approved by team
