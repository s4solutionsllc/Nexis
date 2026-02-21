Perfect! Now I have all the information I need. Let me create the comprehensive research report:

## FR-25 Research Report: Remove QIcon::fromTheme — Use Bundled Assets Only

### Executive Summary

The Nexis codebase currently uses `QIcon::fromTheme()` in 12 locations across 5 source files to load system theme icons with bundled SVG fallbacks. This research documents all occurrences, their usage patterns, platform-specific implementations, and the bundled assets that must replace them for macOS and Linux.

---

### 1. Complete Catalog of QIcon::fromTheme Calls

#### File 1: `/Users/luke/Documents/GitHub/Nexis/shared/nexis/app.cpp`

**Location:** Line 330 (in a lambda function)
```cpp
btn->setIcon(QIcon::fromTheme(sysTheme, QIcon(svgPath)));
```

**Context:** Sidebar button icon initialization with 14 calls to the `setIcon` lambda (lines 335–348)

**Platform Guard:** `#ifdef Q_OS_MAC` / `#else` — ON LINUX ONLY

**Icon Names Requested (14 total):**
1. `"utilities-system-monitor"` (fallback: `dash.svg`)
2. `"computer"` (fallback: `hardware-info.svg`)
3. `"media-playback-start"` (fallback: `startup-apps.svg`)
4. `"edit-clear-all"` (fallback: `cleaner.svg`)
5. `"edit-find"` (fallback: `search.svg`)
6. `"system-run"` (fallback: `services.svg`)
7. `"view-list"` (fallback: `process.svg`)
8. `"preferences-other"` (fallback: `helpers.svg`)
9. `"edit-delete"` (fallback: `uninstaller.svg`)
10. `"preferences-system"` (fallback: `resources.svg`)
11. `"system-software-install"` (fallback: `ppa-manager.svg`)
12. `"preferences-desktop-appearance"` (fallback: `gnome-settings.svg`)
13. `"applications-system"` (fallback: `settings.svg`)
14. `"mail-message-new"` (fallback: `feedback.svg`)

**Bundled Fallbacks:** All 14 fallback SVGs already exist and are bundled in the QRC file:
- `/Users/luke/Documents/GitHub/Nexis/shared/nexis/static/themes/default/img/sidebar-icons/*.svg`
- `/Users/luke/Documents/GitHub/Nexis/shared/nexis/static/themes/light/img/sidebar-icons/*.svg`

**Status:** READY — All fallbacks are pre-bundled; macOS already uses bundled assets only (line 328).

---

#### File 2: `/Users/luke/Documents/GitHub/Nexis/shared/nexis/Pages/SystemCleaner/system_cleaner_page.cpp`

**Location 2a:** Line 22 (constructor initialization list)
```cpp
mDefaultIcon(QIcon::fromTheme("application-x-executable", QIcon(":/static/themes/common/img/package.png"))),
```

**Platform Guard:** `#ifdef Q_OS_MACOS` / `#else` — ON LINUX ONLY

**Icon Name:** `"application-x-executable"`

**Bundled Fallback:** `:/static/themes/common/img/package.png` (exists in QRC)

**Usage:** Set as `mDefaultIcon` member variable for tree items without matching system theme icons

---

**Location 2b:** Line 66 (in a lambda `setThemePixmap`)
```cpp
QIcon icon = QIcon::fromTheme(themeName, QIcon(fallback));
```

**Platform Guard:** `#else` (Linux only, within macOS/else branch starting at line 61)

**Icon Names (6 total, lines 72–77):**
1. `"package-x-generic"` (fallback: `c_package.svg`)
2. `"dialog-warning"` (fallback: `c_crash.svg`)
3. `"text-x-generic"` (fallback: `c_logs.svg`)
4. `"folder"` (fallback: `c_cache.svg`)
5. `"user-trash"` (fallback: `c_trash.svg`)
6. `"applications-development"` (fallback: `c_devtools.svg`)

**Bundled Fallbacks:** All 6 fallback SVGs exist in the QRC file:
- `:/static/themes/common/img/c_*.svg`

**Status:** READY — All fallbacks are pre-bundled; macOS uses custom setPixmap logic (lines 49–60).

---

**Location 2c:** Line 156 (dynamic tree item icon)
```cpp
item->setIcon(0, QIcon::fromTheme(text, mDefaultIcon));
```

**Platform Guard:** `#else` (Linux only, within macOS/else branch starting at line 152)

**Icon Name:** Dynamic — passes filename text to theme lookup (e.g., package names)

**Bundled Fallback:** `mDefaultIcon` (which is set at line 22)

**Usage:** Set icon on individual tree items for file/package names; falls back to `mDefaultIcon`

**Status:** DYNAMIC — Each tree item's filename is passed to theme lookup. On Linux, the system theme is queried for arbitrary file type icons.

---

#### File 3: `/Users/luke/Documents/GitHub/Nexis/shared/nexis/Pages/Resources/disk_usage_launcher_widget.cpp`

**Location 3a:** Line 88 (nested QIcon::fromTheme calls)
```cpp
toolIcon = QIcon::fromTheme("org.gnome.baobab", QIcon::fromTheme("baobab"));
```

**Platform Guard:** `#ifdef Q_OS_LINUX` (Linux only)

**Icon Names:** Primary `"org.gnome.baobab"`, fallback `"baobab"`

**Bundled Fallback:** None provided in inner `QIcon::fromTheme` call

**Status:** MISSING — No bundled fallback for baobab; needs SVG asset creation

---

**Location 3b:** Line 90 (nested QIcon::fromTheme calls)
```cpp
toolIcon = QIcon::fromTheme("org.kde.filelight", QIcon::fromTheme("filelight"));
```

**Platform Guard:** `#ifdef Q_OS_LINUX` (Linux only)

**Icon Names:** Primary `"org.kde.filelight"`, fallback `"filelight"`

**Bundled Fallback:** None provided in inner `QIcon::fromTheme` call

**Status:** MISSING — No bundled fallback for filelight; needs SVG asset creation

---

**Location 3c:** Line 92 (nested QIcon::fromTheme calls)
```cpp
toolIcon = QIcon::fromTheme("qdirstat", QIcon::fromTheme("folder"));
```

**Platform Guard:** `#ifdef Q_OS_LINUX` (Linux only)

**Icon Names:** Primary `"qdirstat"`, fallback `"folder"`

**Bundled Fallback:** `folder.png` exists in QRC (but icon uses theme fallback instead)

**Status:** PARTIAL — `folder.png` exists, but the fallback is theme-based `"folder"` icon name (line 92, nested call)

---

**Location 3d:** Line 94 (fallback call in else branch)
```cpp
toolIcon = QIcon::fromTheme("drive-harddisk");
```

**Platform Guard:** `#ifdef Q_OS_LINUX` (Linux only, as else branch)

**Icon Name:** `"drive-harddisk"`

**Bundled Fallback:** None

**Status:** MISSING — No bundled fallback; only relies on system theme

---

**Location 3e:** Line 96 (macOS call)
```cpp
toolIcon = QIcon::fromTheme("drive-harddisk");
```

**Platform Guard:** `#elif defined(Q_OS_MACOS)` (macOS only)

**Icon Name:** `"drive-harddisk"`

**Bundled Fallback:** None

**Status:** ISSUE — macOS is using `QIcon::fromTheme()`, which is problematic because Adwaita icons are greyscale and Qt doesn't recolor them. This contradicts the app.cpp pattern (line 328) which explicitly avoids theme icons on macOS.

---

#### File 4: `/Users/luke/Documents/GitHub/Nexis/shared/nexis/Pages/Uninstaller/uninstaller_page.cpp`

**Location 4a:** Line 135 (tree item icon)
```cpp
item->setIcon(0, QIcon::fromTheme(pkg.name, fallbackIcon));
```

**Platform Guard:** None (Linux and macOS)

**Icon Name:** Dynamic — package name

**Bundled Fallback:** `fallbackIcon` defined at line 102 as `QIcon(":/static/themes/common/img/package.png")`

**Usage:** Set icon for each package in the APT/Brew packages tree; falls back to package.png

**Status:** PARTIAL FIXED — Has fallback, but still uses `QIcon::fromTheme()` for theme lookup; macOS shouldn't do this

---

**Location 4b:** Line 158 (snap package list item)
```cpp
QListWidgetItem *item = new QListWidgetItem(QIcon::fromTheme(package, icon), QString("  %1").arg(package));
```

**Platform Guard:** None (Linux and macOS)

**Icon Name:** Dynamic — snap package name

**Bundled Fallback:** `icon` defined at line 156 as `QIcon(":/static/themes/common/img/package.png")`

**Usage:** Set icon for each snap package; falls back to package.png

**Status:** PARTIAL FIXED — Has fallback, but still uses `QIcon::fromTheme()` for theme lookup

---

#### File 5: `/Users/luke/Documents/GitHub/Nexis/shared/nexis/Pages/AptSourceManager/apt_source_manager_page.cpp`

**Location 5a:** Line 176 (tree item icon)
```cpp
item->setIcon(0, QIcon::fromTheme(pkg.name, fallbackIcon));
```

**Platform Guard:** None (Linux and macOS)

**Icon Name:** Dynamic — package name (formula/cask name on macOS)

**Bundled Fallback:** `fallbackIcon` defined at line 161 as `QIcon(":/static/themes/common/img/package.png")`

**Usage:** Set icon for each package/formula/cask in the tree; falls back to package.png

**Status:** PARTIAL FIXED — Has fallback, but still uses `QIcon::fromTheme()` for theme lookup

---

### 2. Summary Table of All QIcon::fromTheme Calls

| File | Line | Platform | Icon Name(s) | Fallback | Status |
|------|------|----------|--------------|----------|--------|
| app.cpp | 330 | Linux | 14 theme names | SVGs (bundled) | READY |
| system_cleaner_page.cpp | 22 | Linux | `application-x-executable` | package.png (bundled) | READY |
| system_cleaner_page.cpp | 66 | Linux | 6 theme names | SVGs (bundled) | READY |
| system_cleaner_page.cpp | 156 | Linux | Dynamic (filename) | mDefaultIcon | DYNAMIC |
| disk_usage_launcher_widget.cpp | 88 | Linux | `org.gnome.baobab`, `baobab` | None | MISSING |
| disk_usage_launcher_widget.cpp | 90 | Linux | `org.kde.filelight`, `filelight` | None | MISSING |
| disk_usage_launcher_widget.cpp | 92 | Linux | `qdirstat`, fallback `folder` | None (theme fallback) | PARTIAL |
| disk_usage_launcher_widget.cpp | 94 | Linux | `drive-harddisk` | None | MISSING |
| disk_usage_launcher_widget.cpp | 96 | macOS | `drive-harddisk` | None | ISSUE |
| uninstaller_page.cpp | 135 | All | Dynamic (pkg.name) | package.png (bundled) | PARTIAL |
| uninstaller_page.cpp | 158 | All | Dynamic (package name) | package.png (bundled) | PARTIAL |
| apt_source_manager_page.cpp | 176 | All | Dynamic (pkg.name) | package.png (bundled) | PARTIAL |

**Total Calls:** 12 (some files have multiple calls in different contexts)

---

### 3. Bundled Asset Inventory

#### QRC File Structure
**File:** `/Users/luke/Documents/GitHub/Nexis/shared/nexis/static.qrc`

**Sidebar Icons (both themes):**
- Lines 74–86 (default theme): `dash.svg`, `hardware-info.svg`, `startup-apps.svg`, `cleaner.svg`, `search.svg`, `services.svg`, `process.svg`, `helpers.svg`, `uninstaller.svg`, `resources.svg`, `ppa-manager.svg`, `gnome-settings.svg`, `settings.svg`, `feedback.svg`
- Lines 88–101 (light theme): Same as above

**Common Images (available for both themes):**
- `checkbox.png`, `un-checkbox.png`, `check.png`, `un-check.png`
- `chevron-down.svg`, `chevron-right.svg`, `chevron-left.svg`
- `circle-checked.svg`, `circle-unchecked.svg`
- `sort-asc.svg`, `sort-dsc.svg`
- `c_package.svg`, `c_crash.svg`, `c_logs.svg`, `c_cache.svg`, `c_trash.svg`, `c_devtools.svg`
- `package.png`, `folder.png`, `delete.png`, `trash_2.png`, `donate.png`, `not-found.png`
- `spinup.png`, `spindown.png`, `spinup.svg`, `spindown.svg`

#### Missing Assets (Required for Removal)

1. **Disk usage tool icons (disk_usage_launcher_widget.cpp):**
   - `baobab.svg` (GNOME Disk Usage Analyzer icon)
   - `filelight.svg` (KDE Disk Usage Analyzer icon)
   - `qdirstat.svg` (QDirStat disk usage analyzer icon)
   - `drive-harddisk.svg` (Generic hard disk icon)

**Note:** All 4 are needed for the disk usage launcher widget to work without theme icons.

---

### 4. Platform-Specific Behavior

#### macOS Behavior

**Current State:**
- `app.cpp` line 328: Bypasses `QIcon::fromTheme()` and uses bundled SVGs directly for sidebar buttons
- `system_cleaner_page.cpp` lines 20, 49–60: Bypasses theme for both constructor and init; uses hardcoded bundled SVG paths
- `disk_usage_launcher_widget.cpp` line 96: **INCONSISTENT** — Still uses `QIcon::fromTheme("drive-harddisk")`
- `uninstaller_page.cpp` lines 135, 158: **INCONSISTENT** — Still use `QIcon::fromTheme()` with fallback
- `apt_source_manager_page.cpp` line 176: **INCONSISTENT** — Still uses `QIcon::fromTheme()` with fallback

**Rationale for Bypass:** Comments state that Adwaita symbolic icons are greyscale and Qt doesn't recolor them like GNOME does.

#### Linux Behavior

**Current State:**
- `app.cpp` line 330: Uses system theme with bundled SVG fallback (preferred behavior)
- `system_cleaner_page.cpp` lines 22, 66: Uses system theme with bundled SVG fallback
- `disk_usage_launcher_widget.cpp` lines 88, 90, 92, 94: Uses system theme with nested or missing fallbacks
- `uninstaller_page.cpp` lines 135, 158: Uses system theme with fallback
- `apt_source_manager_page.cpp` line 176: Uses system theme with fallback

---

### 5. Theme Initialization (main.cpp)

**File:** `/Users/luke/Documents/GitHub/Nexis/shared/nexis/main.cpp`

**Lines 106–119 (Theme Setup):**

```cpp
#ifdef Q_OS_MAC
    // macOS: Homebrew-installed icon themes aren't in Qt's default search paths.
    // Add both ARM (/opt/homebrew) and Intel (/usr/local) Homebrew locations.
    {
        QStringList paths = QIcon::themeSearchPaths();
        paths << "/opt/homebrew/share/icons" << "/usr/local/share/icons";
        QIcon::setThemeSearchPaths(paths);
    }
#endif

    // Ensure Adwaita icons are available as a fallback theme
    if (QIcon::themeName().isEmpty())
        QIcon::setThemeName("Adwaita");
    QIcon::setFallbackThemeName("Adwaita");
```

**Key Points:**
1. Sets Adwaita as fallback theme (lines 117–119)
2. Adds Homebrew icon paths on macOS (lines 110–113)
3. **To Remove QIcon::fromTheme completely, these theme setup lines become unnecessary** (but harmless to keep for backward compatibility or future use)

---

### 6. Architecture Notes

#### Icon Resolution Logic

When `QIcon::fromTheme(name, fallback)` is called:
1. Qt searches the current theme (Adwaita, set at line 118 of main.cpp)
2. If not found, falls back to the provided `QIcon` object
3. If no fallback provided, returns an empty/null icon

**For complete removal, we must replace ALL calls such that every path either:**
- Uses a bundled QIcon directly
- Uses a bundled SVG/PNG path
- Or provides a fallback for dynamic names

#### Dynamic Package Name Lookups

Lines 135, 156, 158, 176 attempt to load icons based on package names or executable names. System themes contain icons for common packages (e.g., `firefox`, `chromium`, `libreoffice`). On Linux, these should be preserved or mapped to a generic fallback (e.g., package.png).

**Options for dynamic names:**
1. **Remove theme lookup entirely** — Always use fallback (simplest, but loses themed package icons)
2. **Map to bundled assets** — Create a mapping of common package names to bundled SVGs
3. **Keep theme lookup on Linux only** — Platform-specific code path (more complex but preserves Linux theme icons)

---

### 7. Key Findings & Recommendations

#### Finding 1: Inconsistent macOS Behavior
**Issue:** app.cpp and system_cleaner_page.cpp correctly bypass theme icons on macOS (lines 325–328, 45–60), but uninstaller_page.cpp, disk_usage_launcher_widget.cpp, and apt_source_manager_page.cpp still call `QIcon::fromTheme()` on macOS.

**Impact:** MacOS users may see greyscale Adwaita icons instead of colored fallbacks.

#### Finding 2: Missing Fallbacks in disk_usage_launcher_widget.cpp
**Issue:** Lines 88, 90, 92, 94 attempt to load disk usage tool icons from the theme without fallback SVGs. `baobab`, `filelight`, `qdirstat`, and `drive-harddisk` are not bundled.

**Impact:** If the user's system theme doesn't provide these icons, the widget shows no icon or a placeholder.

#### Finding 3: Dynamic Package Name Icon Lookups
**Issue:** uninstaller_page.cpp, apt_source_manager_page.cpp, and system_cleaner_page.cpp line 156 rely on system theme icons for arbitrary package names, which may not exist or render as expected.

**Impact:** Cross-platform consistency issue; Linux users get varied package icons, macOS/unsupported packages get fallback.

#### Finding 4: All Required Sidebar Icons Are Already Bundled
**Positive:** app.cpp's 14 sidebar button icons are already fully bundled in the QRC for both default and light themes. Removal here is straightforward (just use bundled SVGs on all platforms).

#### Finding 5: Most Cleaner Icons Are Already Bundled
**Positive:** system_cleaner_page.cpp icons (`c_package.svg`, `c_crash.svg`, `c_logs.svg`, `c_cache.svg`, `c_trash.svg`, `c_devtools.svg`) are all pre-bundled. Removal is straightforward.

---

### 8. Action Items for Implementation Phase

1. **Create missing disk usage tool icons:**
   - `baobab.svg` — GNOME Disk Usage icon
   - `filelight.svg` — KDE Disk Usage icon
   - `qdirstat.svg` — QDirStat icon
   - `drive-harddisk.svg` — Generic hard disk icon
   - Add all to QRC file

2. **Update app.cpp line 330:** Replace `QIcon::fromTheme()` with direct bundled SVG path (already done on macOS; extend to Linux)

3. **Update system_cleaner_page.cpp:**
   - Line 22: Replace with direct bundled fallback (already done on macOS; extend to Linux)
   - Line 66: Replace lambda with direct bundled SVG paths
   - Line 156: Replace dynamic theme lookup with mDefaultIcon fallback or generic icon

4. **Update disk_usage_launcher_widget.cpp:**
   - Lines 88, 90, 92, 94, 96: Replace all theme calls with bundled SVG paths
   - Add platform guards to use bundled assets on all platforms

5. **Update uninstaller_page.cpp:**
   - Lines 135, 158: Replace theme lookups with fallback icon (package.png is already adequate)
   - Remove platform inconsistency

6. **Update apt_source_manager_page.cpp:**
   - Line 176: Replace theme lookup with fallback icon

7. **Optionally clean up main.cpp:**
   - Lines 106–119: Can be removed or kept for backward compatibility (doesn't hurt to keep)

8. **Update static.qrc:**
   - Add entries for the 4 new disk usage tool icons

---

### 9. File Location Reference

**Source Files Requiring Changes:**
- `/Users/luke/Documents/GitHub/Nexis/shared/nexis/app.cpp`
- `/Users/luke/Documents/GitHub/Nexis/shared/nexis/Pages/SystemCleaner/system_cleaner_page.cpp`
- `/Users/luke/Documents/GitHub/Nexis/shared/nexis/Pages/Resources/disk_usage_launcher_widget.cpp`
- `/Users/luke/Documents/GitHub/Nexis/shared/nexis/Pages/Uninstaller/uninstaller_page.cpp`
- `/Users/luke/Documents/GitHub/Nexis/shared/nexis/Pages/AptSourceManager/apt_source_manager_page.cpp`
- `/Users/luke/Documents/GitHub/Nexis/shared/nexis/main.cpp` (optional cleanup)

**Resources to Modify:**
- `/Users/luke/Documents/GitHub/Nexis/shared/nexis/static.qrc` (add 4 new icon entries)
- `/Users/luke/Documents/GitHub/Nexis/shared/nexis/static/themes/common/img/` (add 4 new SVG files)

---

### 10. QRC File Relevant Sections

**Lines 74–101:** Sidebar icons (14 total, both default and light themes) — ALL PRESENT

**Lines 68–72:** Common utility icons (c_*.svg) — ALL PRESENT

**Line 58:** `package.png` — Present (used as fallback)

**Line 59:** `folder.png` — Present (used as fallback)

**Missing from QRC (to be added):**
- `baobab.svg`
- `filelight.svg`
- `qdirstat.svg`
- `drive-harddisk.svg`

---

This research conclusively documents all 12 `QIcon::fromTheme()` calls, their fallbacks, platform behavior, and the exact bundled assets that must be created or verified to complete FR-25.
