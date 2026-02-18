I cannot create files as this is a read-only task. However, I've completed the comprehensive research investigation. Let me provide you with the detailed findings directly:

---

# FR-09 Research Report: APT-RPM Support for ALT Linux, PCLinuxOS, Vine Linux

## Executive Summary

APT-RPM is a package management system that brings APT (Advanced Package Tool) compatibility to RPM-based distributions. Unlike standard DNF/YUM on Fedora/RHEL, APT-RPM distributions (ALT Linux, PCLinuxOS, Vine Linux) use **APT as their primary interface backed by RPM internals**. Adding APT-RPM support requires minimal code changes because APT-RPM uses identical APT commands, source file formats, and cache locations as Debian/Ubuntu.

## Current Package Management Architecture

### 1. **Package Tools Enum** 
**File:** `/Users/luke/Documents/GitHub/Nexis/shared/nexis-core/Tools/package_tool_shared.h` (lines 14-23)

```cpp
enum PackageTools {
    APT,        // debian
    DNF,        // fedora
    YUM,        // fedora  
    PACMAN,     // arch
    SNAP,       // snap
    HOMEBREW,   // macOS
    ZYPPER,     // opensuse
    UNKNOWN
};
```

**Key Finding:** The enum currently hardcodes `APT` to mean Debian/Ubuntu only. APT-RPM must be distinguished separately since both use `apt-get`.

### 2. **Package Manager Detection**
**File:** `/Users/luke/Documents/GitHub/Nexis/linux/nexis-core/Tools/package_tool.cpp` (lines 6-12)

```cpp
const PackageTools PackageTool::currentPackageTool =
        CommandUtil::isExecutable("apt-get") ? APT :
        CommandUtil::isExecutable("dnf")     ? DNF :
        CommandUtil::isExecutable("yum")     ? YUM :
        CommandUtil::isExecutable("pacman")  ? PACMAN :
        CommandUtil::isExecutable("zypper")  ? ZYPPER :
                                               UNKNOWN;
```

**Problem:** This detects `apt-get` first, matching both Debian AND APT-RPM systems indiscriminately. A secondary check is needed:
- **Debian/Ubuntu APT:** Has `apt-get` AND `/etc/debian_version` exists
- **APT-RPM:** Has `apt-get` AND `/etc/os-release` contains `ID=alt` OR `ID=pclinuxos` OR `ID=vine`

### 3. **Platform-Specific Package Functions**

#### DEB (Debian) Functions (lines 18-86):
```
getDpkgPackageCaches()     → /var/cache/apt/archives/
getDpkgPackages()          → dpkg-query -W -f (PROBLEM: doesn't exist on APT-RPM)
dpkgRemovePackages()       → apt-get remove/purge (WORKS on APT-RPM)
dpkgDryRunRemove()         → apt-get remove --dry-run (WORKS on APT-RPM)
```

#### RPM (YUM/DNF) Functions (lines 91-153):
```
getRpmPackages()           → rpm -qa --queryformat
getYumDnfPackageCaches()   → /var/cache/dnf/ or /var/cache/yum/
dnfRemovePackages()        → dnf remove
yumRemovePackages()        → yum remove
rpmDryRunRemove()          → dnf remove --assumeno
```

## How APT-RPM Differs from Standard APT (Debian)

| Aspect | Debian/Ubuntu APT | APT-RPM (ALT/PCLinuxOS/Vine) |
|--------|-------------------|-----|
| Package DB | DPKG status in `/var/lib/dpkg/status` | RPM database in `/var/lib/rpm/` |
| List packages | `dpkg-query -W -f` | `rpm -qa` (no dpkg-query) |
| Cache location | `/var/cache/apt/archives/` | `/var/cache/apt/archives/` (same) |
| Sources | `/etc/apt/sources.list[.d]` | `/etc/apt/sources.list[.d]` (same) |
| Remove packages | `apt-get remove/purge` | `apt-get remove/purge` (same) |
| Dry-run | `apt-get remove --dry-run` | `apt-get remove --dry-run` (same) |
| Distro detection | `/etc/debian_version` exists | `/etc/os-release` with ID matching |

**Critical Finding:** APT-RPM uses **the exact same APT commands** as Debian! The only differences are:
1. Package database backend (RPM instead of DPKG)
2. Query method (use `rpm -qa` instead of `dpkg-query`)
3. Distro identification method (`/etc/os-release` vs `/etc/debian_version`)

## APT Source Tool Architecture

**File:** `/Users/luke/Documents/GitHub/Nexis/shared/nexis-core/Tools/apt_source_tool.h` (lines 11-35)

The `APTSource` struct and `AptSourceTool` class already implement everything needed:

```cpp
class APTSource {
    QString filePath;       // /etc/apt/sources.list.d/vscode.list
    bool isSource;          // deb-src vs deb
    QString options;        // [arch=amd64]
    QString uri;            // http://archive.ubuntu.com/ubuntu
    QString distribution;   // jammy, sisyphus, etc
    QString components;     // main, universe, etc
    QString source;         // full line
    bool isActive;          // not commented out
};

static QList<APTSourcePtr> getSourceList();        // Parses /etc/apt/sources.list[.d]
static void removeAPTSource(APTSourcePtr);         // Deletes source line
static void changeStatus(APTSourcePtr, bool);      // Comments/uncomments
static void changeSource(APTSourcePtr, QString);   // Edits source
static void addRepository(const QString &, bool);  // add-apt-repository
```

**Implementation Details** (linux/nexis-core/Tools/apt_source_tool.cpp, lines 78-127):

```cpp
// Scans both /etc/apt/sources.list.d/*.list and /etc/apt/sources.list
QFileInfoList infoList = QDir(APT_SOURCES_LIST_D_PATH).entryInfoList({"*.list"}, QDir::Files, QDir::Time);
infoList.append(QFileInfo(APT_SOURCES_LIST_PATH));

for (const QFileInfo &info : infoList) {
    QStringList fileContent = FileUtil::readListFromFile(info.absoluteFilePath())
        .filter(QRegularExpression("^\\s{0,}#{0,}\\s{0,}deb"));  // Matches deb/deb-src lines
    // Parse: options, URI, distribution, components
}
```

**Key Finding:** This code is **already APT-RPM compatible**. It:
- Reads `/etc/apt/sources.list[.d]` (used by both Debian and APT-RPM)
- Parses `deb`/`deb-src` lines with identical formatting (APT-RPM format matches Debian exactly)
- Uses `add-apt-repository` (works on APT-RPM systems)
- Directly edits files with `tee` (no distro-specific commands)

## ToolManager Dispatch Architecture

**File:** `/Users/luke/Documents/GitHub/Nexis/linux/nexis/Managers/tool_manager.cpp` (lines 45-122)

The ToolManager routes all operations via switch statements:

```cpp
QList<Package> getPackages() {
    switch (PackageTool::currentPackageTool) {
    case APT:
        return PackageTool::getDpkgPackages();   // MUST ADD: case APT_RPM
    case YUM:
    case DNF:
        return PackageTool::getRpmPackages();
    case PACMAN:
        return PackageTool::getPacmanPackages();
    default:
        return QList<Package>();
    }
}

void uninstallPackages(const QStringList &packages, bool purge) {
    switch (PackageTool::currentPackageTool) {
    case APT:
        PackageTool::dpkgRemovePackages(packages, purge);  // MUST ADD: case APT_RPM
    case YUM:
        PackageTool::yumRemovePackages(packages);
    case DNF:
        PackageTool::dnfRemovePackages(packages);
    // ...
    }
}

QStringList dryRunRemovePackages(const QStringList &packages) {
    switch (PackageTool::currentPackageTool) {
    case APT:
        return PackageTool::dpkgDryRunRemove(packages);  // MUST ADD: case APT_RPM
    case YUM:
    case DNF:
        return PackageTool::rpmDryRunRemove(packages);
    // ...
    }
}

QFileInfoList getPackageCaches() {
    switch (PackageTool::currentPackageTool) {
    case APT:
        return PackageTool::getDpkgPackageCaches();  // MUST ADD: case APT_RPM
    case YUM:
    case DNF:
        return PackageTool::getYumDnfPackageCaches();
    // ...
    }
}
```

## Package Listing: Current Methods

### DEB (Debian) — lines 41-68
```cpp
QString output = CommandUtil::exec("bash", {"-c",
    "dpkg-query -W -f '${Package}\\t${Section}\\t${binary:Summary}\\n' 2> /dev/null"}, {}, 60000);
```

**Problem:** `dpkg-query` doesn't exist on APT-RPM systems.

### RPM (YUM/DNF) — lines 91-119
```cpp
QString output = CommandUtil::exec("bash", {"-c",
    "rpm -qa --queryformat '%{NAME}\\t%{GROUP}\\t%{SUMMARY}\\n' 2> /dev/null"}, {}, 60000);
```

**Works on APT-RPM!** APT-RPM systems have RPM database, so this command works.

### Recommendation for APT-RPM

Use `rpm -qa --queryformat` for APT-RPM (same as existing RPM code) because:
1. APT-RPM distributions use RPM as the backend database
2. `rpm -qa` is guaranteed to work on all APT-RPM systems
3. Minimal code changes (reuse existing `getRpmPackages()` logic)
4. Consistent with how RPM packages are already queried

## Package Removal: Current Methods

### DEB — lines 71-85
```cpp
bool PackageTool::dpkgRemovePackages(QStringList packages, bool purge) {
    packages.insert(0, purge ? "purge" : "remove");
    packages.insert(1, "-y");
    CommandUtil::sudoExec("apt-get", packages);  // apt-get remove/purge -y pkg1 pkg2
}

QStringList PackageTool::dpkgDryRunRemove(const QStringList &packages) {
    QString output = CommandUtil::exec("bash", args)
        .arg(packages.join(' '));  // bash -c "apt-get remove --dry-run pkg1 pkg2"
}
```

### APT-RPM — No New Implementation Needed!

APT-RPM can use **the exact same functions** because:
```bash
apt-get remove -y package1 package2          # Works on APT-RPM
apt-get purge -y package1 package2           # Works on APT-RPM  
apt-get remove --dry-run package1 package2   # Works on APT-RPM
```

These are identical to Debian APT commands!

## Distro Detection Strategy

### Current Problem
Only detects `apt-get` presence; doesn't distinguish Debian from APT-RPM.

### Proposed Solution

Add APT-RPM detection based on `/etc/os-release`:

```cpp
// New helper function for APT-RPM detection
static bool isAptRpm() {
    QFile file("/etc/os-release");
    if (!file.open(QIODevice::ReadOnly)) return false;
    
    QString content = QString::fromUtf8(file.readAll());
    file.close();
    
    // Match ID= or ID_LIKE= fields
    QRegularExpression idRegex("ID(_LIKE)?=([\"']?)([^\"'\\n]+)\\2");
    QRegularExpressionMatch match = idRegex.match(content);
    if (match.hasMatch()) {
        QString id = match.captured(3);
        return id.contains("altlinux", Qt::CaseInsensitive) ||
               id.contains("pclinuxos", Qt::CaseInsensitive) ||
               id.contains("vine", Qt::CaseInsensitive);
    }
    return false;
}

// Updated detection logic
const PackageTools PackageTool::currentPackageTool =
        (CommandUtil::isExecutable("apt-get") && QFile("/etc/debian_version").exists()) ? APT :
        (CommandUtil::isExecutable("apt-get") && isAptRpm()) ? APT_RPM :  // NEW
        CommandUtil::isExecutable("dnf")     ? DNF :
        CommandUtil::isExecutable("yum")     ? YUM :
        CommandUtil::isExecutable("pacman")  ? PACMAN :
        CommandUtil::isExecutable("zypper")  ? ZYPPER :
                                               UNKNOWN;
```

### Example /etc/os-release Files

**ALT Linux:**
```
ID=alt
NAME="ALT Linux"
PRETTY_NAME="ALT Linux sisyphus"
```

**PCLinuxOS:**
```
ID=pclinuxos
NAME="PCLinuxOS"
```

**Vine Linux:**
```
ID=vine
NAME="Vine Linux"
```

## Summary of Changes Required

### 1. **shared/nexis-core/Tools/package_tool_shared.h**
- Add `APT_RPM` enum value after `APT`

### 2. **linux/nexis-core/Tools/package_tool.cpp**
- Add `isAptRpm()` helper function (~15 lines)
- Update `currentPackageTool` initialization (~1 line change)
- No new package functions needed — reuse existing ones

### 3. **linux/nexis/Managers/tool_manager.cpp**
- Add `case APT_RPM:` to 4 switch statements (getPackages, uninstallPackages, dryRunRemovePackages, getPackageCaches)
- Route APT_RPM to same functions as APT (~8 total lines added)

### 4. **Files That Need NO Changes**
- `apt_source_tool.*` — Already generic, works on APT-RPM
- `apt_source_manager_page.*` — Already uses ToolManager abstraction
- macOS files — N/A
- Shared headers — Already abstract

## Code Complexity & Risk Assessment

| Component | Changes | Risk |
|-----------|---------|------|
| Enum expansion | 1 line | Trivial |
| APT-RPM detection | ~15 lines | Low (uses standard /etc/os-release) |
| ToolManager routing | ~8 lines | Trivial (copy-paste cases) |
| Package functions | 0 lines | None (reuse existing code) |
| Source management | 0 lines | None (already generic) |
| UI | 0 lines | None (abstracted) |

**Total Effort:** ~25 lines of actual code changes across 2 files.

**Risk Level:** **LOW** because:
- APT-RPM uses identical APT command syntax
- Source file format identical to Debian
- Detection method uses standard `/etc/os-release` (widely adopted)
- Easy to test on actual ALT/PCLinuxOS/Vine systems
- Fallback to UNKNOWN is safe if detection fails

## Files with Line References

**Key files affecting APT-RPM support:**

1. **`/Users/luke/Documents/GitHub/Nexis/shared/nexis-core/Tools/package_tool_shared.h`** — enum (lines 14-23)
2. **`/Users/luke/Documents/GitHub/Nexis/linux/nexis-core/Tools/package_tool.cpp`** — detection + functions (lines 6-12, 41-68, 71-85, 91-119)
3. **`/Users/luke/Documents/GitHub/Nexis/linux/nexis/Managers/tool_manager.cpp`** — dispatch (lines 45-122)
4. **`/Users/luke/Documents/GitHub/Nexis/shared/nexis-core/Tools/apt_source_tool.cpp`** — NO CHANGES NEEDED (already generic, lines 78-127)
5. **`/Users/luke/Documents/GitHub/Nexis/shared/nexis/Pages/AptSourceManager/apt_source_manager_page.cpp`** — NO CHANGES NEEDED (abstracted, lines 101-132)

## Testing Strategy

Once implemented:
1. Test detection on actual ALT Linux, PCLinuxOS, and Vine Linux systems
2. Verify `PackageTool::currentPackageTool == APT_RPM` on these distros
3. List packages and confirm display in Uninstaller page
4. Test cache size calculation from `/var/cache/apt/archives/`
5. Remove/purge packages via Uninstaller
6. Test dry-run to show dependencies
7. Verify Repository Manager add/edit/delete operations
8. Trace `pkexec` calls to confirm correct `apt-get` usage

---

This research is ready for Phase 2 (Planning) when you're ready to proceed with implementation. The changes are minimal and low-risk due to APT-RPM's command-level compatibility with Debian APT.
