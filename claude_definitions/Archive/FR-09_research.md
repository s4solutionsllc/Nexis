# FR-09 Research Report: APT-RPM Support for ALT Linux, PCLinuxOS, Vine Linux

## Executive Summary

APT-RPM is a port of Debian's APT package management system adapted to work with RPM packages instead of `.deb` packages. It provides the familiar `apt-get`/`apt-cache` CLI but manages `.rpm` files via the RPM database. Three distributions use it: ALT Linux (actively maintained, Russian enterprise focus), PCLinuxOS (transitioning to DNF5), and Vine Linux (largely discontinued since 2021).

QuentiumYT/Stacer merged PR #31 implementing this feature with +69/-24 lines across 7 files. The approach is straightforward: detect APT-RPM via dual presence of `apt-get` + `rpm`, then replace hardcoded `"deb"`/`"deb-src"` strings with dynamic `binaryType()`/`sourceType()` helper functions that return `"rpm"`/`"rpm-src"` on APT-RPM systems.

## What is APT-RPM?

APT-RPM uses APT as a frontend layer on top of the RPM package system. While Debian APT uses dpkg as its backend to install/remove `.deb` packages, APT-RPM uses `rpm` as its backend for `.rpm` packages.

- **Commands:** Same as Debian (`apt-get install`, `apt-get remove`, `apt-cache search`)
- **Package database:** RPM database at `/var/lib/rpm/` (not dpkg at `/var/lib/dpkg/`)
- **Cache location:** `/var/cache/apt/archives/` (same as Debian)
- **Config files:** `/etc/apt/sources.list` and `/etc/apt/sources.list.d/` (same paths as Debian)

### Source Line Format Differences

| Feature | Debian APT | APT-RPM |
|---------|-----------|---------|
| Binary keyword | `deb` | `rpm` |
| Source keyword | `deb-src` | `rpm-src` |
| Repomd keyword | N/A | `repomd` / `repomd-src` |
| Local repos | `file://` with `deb` | `rpm-dir` / `rpm-src-dir` |

**Debian example:**
```
deb http://archive.ubuntu.com/ubuntu jammy main universe
```

**APT-RPM example (ALT Linux):**
```
rpm [p10] http://mirror.yandex.ru/altlinux/ p10/branch/x86_64-i586 classic
rpm [Sisyphus] http://ftp.altlinux.org/pub/distributions/ALTLinux/Sisyphus noarch classic
```

### Distribution Status

| Distribution | Status | Notes |
|-------------|--------|-------|
| **ALT Linux** | Actively maintained | Russian enterprise distro. Has unique `apt-repo` CLI tool for managing sources. Regular releases through 2025-2026. |
| **PCLinuxOS** | Active but transitioning | Rolling release. Now shipping DNF5 as default package manager (since 2025.08). APT-RPM/Synaptic available as legacy. |
| **Vine Linux** | Largely discontinued | Japanese distro. All stable versions discontinued May 2021. Only VineSeed (rolling) may have minimal activity. |

## Current Nexis Architecture Analysis

### Package Manager Detection
**File:** `linux/nexis-core/Tools/package_tool.cpp` (line 6-12)

```cpp
const PackageTools PackageTool::currentPackageTool =
        CommandUtil::isExecutable("apt-get") ? APT :
        CommandUtil::isExecutable("dnf")     ? DNF :
        CommandUtil::isExecutable("yum")     ? YUM :
        CommandUtil::isExecutable("pacman")  ? PACMAN :
        CommandUtil::isExecutable("zypper")  ? ZYPPER :
                                               UNKNOWN;
```

**Problem:** Detects `apt-get` first, matching both Debian AND APT-RPM systems as `APT`. On APT-RPM systems, `dpkg-query` doesn't exist so `getDpkgPackages()` fails silently.

### Enum Definition
**File:** `shared/nexis-core/Tools/package_tool_shared.h` (line 14-23)

```cpp
enum PackageTools {
    APT, DNF, YUM, PACMAN, SNAP, HOMEBREW, ZYPPER, UNKNOWN
};
```

No `APT_RPM` enum value exists.

### ToolManager Dispatch (Linux)
**File:** `linux/nexis/Managers/tool_manager.cpp`

5 switch statements route to platform-specific implementations:
- `getPackages()` — line 47: `case APT: return getDpkgPackages();`
- `dryRunRemovePackages()` — line 72: `case APT: return dpkgDryRunRemove();`
- `getPackageCaches()` — line 87: `case APT: return getDpkgPackageCaches();`
- `uninstallPackages()` — line 106: `case APT: dpkgRemovePackages();`

Plus the disk usage launcher widget (line 432): `case APT: apt-get install`

### APT Source Tool — Hardcoded `"deb"` References
**File:** `linux/nexis-core/Tools/apt_source_tool.cpp`

6 locations with hardcoded `"deb"` or `"deb-src"`:

1. **Line 58** — `changeSource()` deb822 stanza matching: `aptSource->isSource ? "deb-src" : "deb"`
2. **Line 70** — `changeSource()` deb822 field update: `fields["Types"] = newSource->isSource ? "deb-src" : "deb"`
3. **Line 184** — `changeSource()` .list line reconstruction: `newSource->isSource ? "deb-src" : "deb"`
4. **Line 243** — `getSourceList()` deb822 type check: `types.contains("deb")`
5. **Line 253** — `getSourceList()` deb822 synthetic source: `aptSource->isSource ? "deb-src" : "deb"`
6. **Line 278** — `getSourceList()` .list filter regex: `"^\\s{0,}#{0,}\\s{0,}deb"`
7. **Line 296** — `getSourceList()` .list type check: `sourceColumns.first() == "deb"`
8. **Line 297** — `getSourceList()` .list type check: `sourceColumns.first() == "deb-src"`

### APT Source Tool — `addRepository()` Method
**File:** `linux/nexis-core/Tools/apt_source_tool.cpp` (line 21-29)

Currently uses `add-apt-repository` which doesn't exist on APT-RPM. ALT Linux uses `apt-repo` instead; PCLinuxOS has no equivalent tool.

### Repository Item Display
**File:** `shared/nexis/Pages/AptSourceManager/apt_source_repository_item.cpp` (line 43-51)

Strips bracket options from display. Uses `mAptSource->source` directly — will work with `rpm` prefix as-is.

## QuentiumYT PR #31 Analysis

### Detection Strategy

QuentiumYT uses a simple heuristic: both `apt-get` AND `rpm` being executable.

```cpp
static bool isAptRpm() {
    return CommandUtil::isExecutable("apt-get") && CommandUtil::isExecutable("rpm");
}
```

**Placed BEFORE the APT check** in the detection chain to take priority.

**Potential issue:** On Debian systems with `rpm` installed (via `apt install rpm`), this could false-positive. However, this is an uncommon configuration.

### Alternative Detection: QuentiumYT vs `/etc/os-release`

| Method | Pros | Cons |
|--------|------|------|
| `apt-get` + `rpm` executable check | Simple, runtime-only, no file parsing | False positive if Debian has `rpm` installed |
| `/etc/os-release` ID check | Precise, explicit, no false positives | Requires knowing all APT-RPM distro IDs, file parsing |
| `apt-get` without `dpkg` | Accurate (APT-RPM systems lack dpkg) | PCLinuxOS _might_ have dpkg installed |

**Recommendation:** Use QuentiumYT's approach (check `apt-get` + `rpm`, place before APT check) with an additional safety guard: check that `dpkg` is NOT present. This eliminates the Debian-with-rpm-installed false positive:

```cpp
CommandUtil::isExecutable("apt-get") && CommandUtil::isExecutable("rpm") && !CommandUtil::isExecutable("dpkg")
```

### Source Line Handling

QuentiumYT adds two helper functions:

```cpp
static QString binaryType() { return isAptRpm() ? "rpm" : "deb"; }
static QString sourceType() { return isAptRpm() ? "rpm-src" : "deb-src"; }
```

Then replaces all hardcoded `"deb"` / `"deb-src"` with calls to these functions.

### Repository Add/Remove

QuentiumYT uses `apt-repo` (ALT Linux tool) when available, falling back to direct file editing:

```cpp
if (isAptRpm() && CommandUtil::isExecutable("apt-repo")) {
    CommandUtil::sudoExec("apt-repo", {"add", source});
} else {
    CommandUtil::sudoExec("add-apt-repository", {"-y", repository});
}
```

### Package Manager Routing

QuentiumYT routes `APT_RPM` as follows:
- **Package listing:** Falls through to `getRpmPackages()` (uses `rpm -qa`)
- **Package cache:** Falls through to `getDpkgPackageCaches()` (APT cache at `/var/cache/apt/archives/`)
- **Package removal:** Falls through to `dpkgRemovePackages()` (uses `apt-get remove`)

This makes sense because:
- Packages are RPMs → list with `rpm -qa`
- APT manages the cache → cache is at `/var/cache/apt/archives/`
- APT handles dependency resolution and removal → `apt-get remove`

### Edit Button

PR discussion notes that `apt-repo` cannot **edit** repositories (only add/remove). QuentiumYT decided to keep the edit button enabled since direct file editing still works, but noted it could be hidden for APT-RPM if desired.

## Key Differences from Existing Nexis Code

1. **No deb822 support needed for APT-RPM:** APT-RPM systems only use `.list` format (no `.sources` files). The deb822 format is Debian-specific. However, the deb822 code should still work generically if an APT-RPM system ever uses it.

2. **Source line type keywords differ:** Must replace hardcoded `"deb"` with `binaryType()` and `"deb-src"` with `sourceType()` throughout `apt_source_tool.cpp`.

3. **Dry-run parsing is the same:** APT-RPM's `apt-get remove --dry-run` produces the same output format as Debian APT (lines starting with `Remv`), so `dpkgDryRunRemove()` works unchanged.

4. **Purge may not be supported:** APT-RPM may not support `apt-get purge`. Need to handle gracefully (fall back to `remove`).

## Impact Summary

| File | Changes Needed |
|------|---------------|
| `shared/nexis-core/Tools/package_tool_shared.h` | Add `APT_RPM` enum |
| `linux/nexis-core/Tools/package_tool.cpp` | Add detection logic (before APT check) |
| `linux/nexis-core/Tools/apt_source_tool.cpp` | Replace hardcoded `"deb"`/`"deb-src"` with dynamic functions; update `addRepository()` for `apt-repo` |
| `linux/nexis/Managers/tool_manager.cpp` | Add `case APT_RPM:` to 4 switch statements |
| `shared/nexis/Pages/Resources/disk_usage_launcher_widget.cpp` | Add `case APT_RPM:` (falls through to APT) |
| `shared/nexis/Pages/AptSourceManager/apt_source_manager_page.cpp` | Update placeholder text for APT-RPM |
| `shared/nexis/Pages/AptSourceManager/apt_source_edit.ui` | Update "Source"/"Binary" radio labels |

**Total estimated effort:** ~70 lines of changes across 7 files.
**Risk level:** LOW — APT-RPM command compatibility with Debian APT makes this largely a routing/string-replacement exercise.
