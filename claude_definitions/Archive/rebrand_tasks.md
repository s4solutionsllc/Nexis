# Rebrand Tasks: Stacer -> Nexis

All references to "Stacer" / "stacer" found in the tracked codebase (excludes `build/` and `output/` which are generated).

---

## 1. Files Candidates for Removal

These files are never referenced in code or build scripts and can likely be deleted outright.

### Old Screenshots (v1.0.9) — Not referenced anywhere
- `screenshots/Screenshot-1.0.9-1.png`
- `screenshots/Screenshot-1.0.9-2.png`
- `screenshots/Screenshot-1.0.9-3.png`
- `screenshots/Screenshot-1.0.9-4.png`
- `screenshots/Screenshot-1.0.9-5.png`
- `screenshots/Screenshot-1.0.9-6.png`
- `screenshots/Screenshot-1.0.9-7.png`
- `screenshots/Screenshot-1.0.9-8.png`
- `screenshots/Screenshot-1.0.9-9.png`
- `screenshots/Screenshot-1.0.9-10.png`
- `screenshots/Screenshot-1.0.9-11.png`
- `screenshots/Screenshot-1.0.9-12.png`
- `screenshots/Screenshot-1.0.9-13.png`
- `screenshots/Screenshot-1.0.9-14.png`
- `screenshots/Screenshot-1.0.9-15.png`
- `screenshots/Screenshot-1.0.9-16.png`

### Branded Screenshots (v2.0.1) — Referenced only in README.md, will need re-capture with new branding
- `screenshots/Stacer-2.0.1-01.png`
- `screenshots/Stacer-2.0.1-02.png`
- `screenshots/Stacer-2.0.1-03.png`
- `screenshots/Stacer-2.0.1-04.png`
- `screenshots/Stacer-2.0.1-05.png`
- `screenshots/Stacer-2.0.1-06.png`
- `screenshots/Stacer-2.0.1-07.png`
- `screenshots/Stacer-2.0.1-08.png`
- `screenshots/Stacer-2.0.1-09.png`
- `screenshots/Stacer-2.0.1-10.png`
- `screenshots/Stacer-2.0.1-11.png`

### Duplicate Icon Sets — `icons/` is a copy of `shared/icons/`; only `icons/` is used by Debian packaging
- `shared/icons/hicolor/16x16/apps/stacer.png`
- `shared/icons/hicolor/32x32/apps/stacer.png`
- `shared/icons/hicolor/64x64/apps/stacer.png`
- `shared/icons/hicolor/128x128/apps/stacer.png`
- `shared/icons/hicolor/256x256/apps/stacer.png`

> **Note:** `icons/hicolor/*/apps/stacer.png` (root-level) are used by Debian packaging via `linux/debian/install` and need renaming, not removal.

---

## 2. Files That Need Renaming

These files have "stacer" in their filename and are actively used.

### macOS App Icon
- `macos/stacer.icns` -> `macos/nexis.icns`

### Linux Icons (used by Debian packaging and AppImage)
- `icons/hicolor/16x16/apps/stacer.png` -> `nexis.png`
- `icons/hicolor/32x32/apps/stacer.png` -> `nexis.png`
- `icons/hicolor/64x64/apps/stacer.png` -> `nexis.png`
- `icons/hicolor/128x128/apps/stacer.png` -> `nexis.png`
- `icons/hicolor/256x256/apps/stacer.png` -> `nexis.png`

### Linux Desktop Entry
- `linux/applications/stacer.desktop` -> `linux/applications/nexis.desktop`

### Translation Files (26 files)
- `shared/translations/stacer_ar.ts` -> `nexis_ar.ts`
- `shared/translations/stacer_ca-es.ts` -> `nexis_ca-es.ts`
- `shared/translations/stacer_cs.ts` -> `nexis_cs.ts`
- `shared/translations/stacer_de.ts` -> `nexis_de.ts`
- `shared/translations/stacer_en.ts` -> `nexis_en.ts`
- `shared/translations/stacer_es.ts` -> `nexis_es.ts`
- `shared/translations/stacer_fr.ts` -> `nexis_fr.ts`
- `shared/translations/stacer_gl.ts` -> `nexis_gl.ts`
- `shared/translations/stacer_hi.ts` -> `nexis_hi.ts`
- `shared/translations/stacer_hu.ts` -> `nexis_hu.ts`
- `shared/translations/stacer_it.ts` -> `nexis_it.ts`
- `shared/translations/stacer_kn.ts` -> `nexis_kn.ts`
- `shared/translations/stacer_ko.ts` -> `nexis_ko.ts`
- `shared/translations/stacer_ml.ts` -> `nexis_ml.ts`
- `shared/translations/stacer_nl.ts` -> `nexis_nl.ts`
- `shared/translations/stacer_oc.ts` -> `nexis_oc.ts`
- `shared/translations/stacer_pl.ts` -> `nexis_pl.ts`
- `shared/translations/stacer_pt.ts` -> `nexis_pt.ts`
- `shared/translations/stacer_ro.ts` -> `nexis_ro.ts`
- `shared/translations/stacer_ru.ts` -> `nexis_ru.ts`
- `shared/translations/stacer_sv.ts` -> `nexis_sv.ts`
- `shared/translations/stacer_tr.ts` -> `nexis_tr.ts`
- `shared/translations/stacer_ua.ts` -> `nexis_ua.ts`
- `shared/translations/stacer_vn.ts` -> `nexis_vn.ts`
- `shared/translations/stacer_zh-cn.ts` -> `nexis_zh-cn.ts`
- `shared/translations/stacer_zh-tw.ts` -> `nexis_zh-tw.ts`

### Core Library Header
- `shared/stacer-core/stacer-core_global.h` -> rename or keep path but update internal macros

### Source Directories (major rename — impacts all `#include` paths and CMake)
- `shared/stacer-core/` -> `shared/nexis-core/` (or similar)
- `shared/stacer/` -> `shared/nexis/` (or similar)
- `linux/stacer-core/` -> `linux/nexis-core/`
- `linux/stacer/` -> `linux/nexis/`
- `macos/stacer-core/` -> `macos/nexis-core/`
- `macos/stacer/` -> `macos/nexis/`

---

## 3. Code References That Need Updating

### CMakeLists.txt (root)
| Line | Current Value | Change To |
|------|--------------|-----------|
| 2 | `project(Stacer VERSION 2.1.3)` | `project(Nexis VERSION 1.0.1)` |
| 33 | Comment: `# stacer-core` | `# nexis-core` |
| 35 | `set(CORE_SHARED_DIR "${SHARED_DIR}/stacer-core")` | `.../nexis-core` |
| 36 | `set(CORE_PLAT_DIR "${PLATFORM_DIR}/stacer-core")` | `.../nexis-core` |
| 55 | `add_definitions(-DSTACERCORE_LIBRARY)` | `-DNEXISCORE_LIBRARY` |
| 59 | `add_library(stacer-core STATIC ...)` | `nexis-core` |
| 63 | `target_link_libraries(stacer-core ...)` | `nexis-core` |
| 69 | `target_link_libraries(stacer-core ...)` | `nexis-core` |
| 73 | Comment: `# stacer (executable)` | `# nexis` |
| 75 | `set(GUI_SHARED_DIR "${SHARED_DIR}/stacer")` | `.../nexis` |
| 76 | `set(GUI_PLAT_DIR "${PLATFORM_DIR}/stacer")` | `.../nexis` |
| 109 | `file(GLOB_RECURSE STACER_TRANSLATIONS ...)` | `NEXIS_TRANSLATIONS` |
| 111 | `qt_create_translation(... STACER_TRANSLATIONS ...)` | `NEXIS_TRANSLATIONS` |
| 136 | `set(MACOS_ICON ".../macos/stacer.icns")` | `.../nexis.icns` |
| 140 | `add_executable(stacer ...)` | `nexis` |
| 149-150 | `target_link_libraries(stacer stacer-core ...)` | `nexis nexis-core` |
| 177 | `target_compile_definitions(stacer ...)` | `nexis` |
| 183 | `target_compile_options(stacer ...)` | `nexis` |
| 191 | `set_target_properties(stacer ...)` | `nexis` |
| 193 | `MACOSX_BUNDLE_GUI_IDENTIFIER "com.stacer.app"` | `"com.nexis.app"` |
| 194 | `MACOSX_BUNDLE_BUNDLE_NAME "Stacer"` | `"Nexis"` |
| 197 | `MACOSX_BUNDLE_ICON_FILE "stacer"` | `"nexis"` |
| 201 | `TARGETS stacer` | `nexis` |
| 209 | `TARGETS stacer` | `nexis` |
| 215 | `FILES ".../stacer.desktop"` | `.../nexis.desktop` |
| 224 | `RENAME stacer.png` | `nexis.png` |

### shared/stacer/main.cpp
| Line | Current Value | Change To |
|------|--------------|-----------|
| 43 | `"/stacer.log"` | `"/nexis.log"` |
| 61 | `qApp->setApplicationName("stacer")` | `"nexis"` |
| 62 | `qApp->setApplicationDisplayName("Stacer")` | `"Nexis"` |
| 67 | `"/stacer.lock"` | `"/nexis.lock"` |
| 72 | `QMessageBox title "Stacer"` | `"Nexis"` |
| 73 | `"Another instance of Stacer..."` | `"...of Nexis..."` |
| 78 | `"Hide Stacer while launching."` | `"Hide Nexis..."` |

### shared/stacer/feedback.cpp
| Line | Current Value | Change To |
|------|--------------|-----------|
| 8 | `"https://github.com/lsimpsonsfdc/Stacer/issues/..."` | Update repo URL to Nexis |

### shared/stacer/Pages/Dashboard/dashboard_page.cpp
| Line | Current Value | Change To |
|------|--------------|-----------|
| 128 | `"https://api.github.com/repos/lsimpsonsfdc/Stacer/releases/latest"` | Update repo URL |
| 155 | `"https://github.com/lsimpsonsfdc/Stacer/releases/latest"` | Update repo URL |

### shared/stacer/Managers/app_manager.cpp
| Line | Current Value | Change To |
|------|--------------|-----------|
| 27 | `QString("stacer_%1").arg(...)` | `"nexis_%1"` |

### shared/stacer/Pages/Helpers/host_manage.cpp
| Line | Current Value | Change To |
|------|--------------|-----------|
| 181 | `"/tmp/stacer_etc_host_new_content"` | `"/tmp/nexis_etc_host_new_content"` |
| 183 | `"/tmp/stacer_etc_host_new_content"` | `"/tmp/nexis_etc_host_new_content"` |

### macos/stacer/Pages/Settings/settings_page.cpp
| Line | Current Value | Change To |
|------|--------------|-----------|
| 26 | `"Stacer v%1"` in HTML | `"Nexis v%1"` |
| 66 | `"/com.stacer.app.plist"` | `"/com.nexis.app.plist"` |
| 140 | `"com.stacer.app"` in plist template | `"com.nexis.app"` |
| 143 | `"stacer"` in plist executable | `"nexis"` |

### linux/stacer/Pages/Settings/settings_page.cpp
| Line | Current Value | Change To |
|------|--------------|-----------|
| 26 | `"Stacer v%1"` in HTML | `"Nexis v%1"` |
| 66 | `"/stacer.desktop"` | `"/nexis.desktop"` |
| 137 | `"Name=Stacer\n"` | `"Name=Nexis\n"` |
| 139 | `"Exec=stacer --hide \n"` | `"Exec=nexis --hide \n"` |

### shared/stacer/app.ui
| Line | Current Value | Change To |
|------|--------------|-----------|
| 17 | `<string notr="true">Stacer</string>` (window title) | `Nexis` |

### shared/stacer/Pages/Settings/settings_page.ui
| Line | Current Value | Change To |
|------|--------------|-----------|
| 171 | `"Autostart Stacer"` | `"Autostart Nexis"` |
| 236 | `"Stacer"` in about HTML | `"Nexis"` |

### shared/stacer-core/stacer-core_global.h
| Line | Current Value | Change To |
|------|--------------|-----------|
| 1 | `#ifndef STACERCORE_GLOBAL_H` | `NEXISCORE_GLOBAL_H` |
| 2 | `#define STACERCORE_GLOBAL_H` | `NEXISCORE_GLOBAL_H` |
| 6 | `#if defined(STACERCORE_LIBRARY)` | `NEXISCORE_LIBRARY` |
| 7 | `#define STACERCORESHARED_EXPORT` | `NEXISCORESHARED_EXPORT` |
| 9 | `#define STACERCORESHARED_EXPORT` | `NEXISCORESHARED_EXPORT` |
| 12 | `#endif // STACERCORE_GLOBAL_H` | `NEXISCORE_GLOBAL_H` |

### All Header Files Using the Core Export Macro (15 files)
Every header that includes `stacer-core_global.h` and uses `STACERCORESHARED_EXPORT` needs updating:
- `shared/stacer-core/Info/cpu_info.h` (lines 9, 11)
- `shared/stacer-core/Info/disk_info.h` (lines 8, 12)
- `shared/stacer-core/Info/gpu_info.h` (lines 7, 20)
- `shared/stacer-core/Info/memory_info.h` (lines 6, 8)
- `shared/stacer-core/Info/network_info.h` (lines 8, 10)
- `shared/stacer-core/Info/process.h` (lines 6, 8)
- `shared/stacer-core/Info/process_info.h` (lines 10, 12)
- `shared/stacer-core/Info/system_info.h` (lines 9, 11)
- `shared/stacer-core/Info/thermal_info.h` (lines 7, 18)
- `shared/stacer-core/Utils/command_util.h` (lines 6, 8)
- `shared/stacer-core/Utils/file_util.h` (lines 13, 15)
- `shared/stacer-core/Utils/format_util.h` (lines 4, 6)
- `shared/stacer-core/Tools/gnome_settings_tool.h` (lines 7, 10)
- `shared/stacer-core/Tools/service_tool.h` (lines 6, 8, 19)
- `linux/stacer-core/Tools/package_tool.h` (lines 5, 10)
- `macos/stacer-core/Tools/package_tool.h` (lines 5, 10)

### linux/applications/stacer.desktop
| Line | Current Value | Change To |
|------|--------------|-----------|
| 2 | `Name=Stacer` | `Name=Nexis` |
| 3 | `Exec=stacer` | `Exec=nexis` |
| 5 | `Icon=stacer` | `Icon=nexis` |

---

## 4. Debian Packaging

### linux/debian/control
| Line | Current Value | Change To |
|------|--------------|-----------|
| 1 | `Source: stacer` | `Source: nexis` |
| 16 | `Homepage: .../Stacer` | Update URL |
| 17 | `Vcs-Browser: .../Stacer.git` | Update URL |
| 19 | `Package: stacer` | `Package: nexis` |
| 24 | `Stacer is an open-source...` | `Nexis is...` |

### linux/debian/copyright
| Line | Current Value | Change To |
|------|--------------|-----------|
| 2 | `Upstream-Name: stacer` | `nexis` |
| 3 | `Source: .../Stacer/` | Update URL |

### linux/debian/changelog
Keep all existing `stacer (x.y.z-n)` entries as historical record. Add a new entry at the top:
```
nexis (1.0.1-1) unstable; urgency=medium

  * Rebrand from Stacer to Nexis
  * Reset version numbering

 -- Luke Simpson <...>  <date>
```
**Do NOT modify or rename the old stacer entries.**

---

## 5. CI/CD Workflows

### .github/workflows/build.yml
| Line | Current Value | Change To |
|------|--------------|-----------|
| 20 | `artifact-name: stacer-linux-x64` | `nexis-linux-x64` |
| 21 | `artifact-path: build/output/stacer` | `build/output/nexis` |
| 24 | `artifact-name: stacer-macos-arm64` | `nexis-macos-arm64` |
| 25 | `artifact-path: build/output/stacer.app` | `build/output/nexis.app` |

### .github/workflows/release.yml
All references to `stacer` in artifact names, binary paths, DMG volume names, AppImage names, clone URLs, install instructions, and release notes (lines 65-66, 87-88, 95, 106, 134, 138, 140-141, 146-147, 153, 184, 197, 200, 237, 257, 276-277, 282, 287, 290, 292, 300-301, 306).

### .github/ISSUE_TEMPLATE/bug_report.yml
| Line | Current Value | Change To |
|------|--------------|-----------|
| 22 | `label: Stacer Version` | `Nexis Version` |
| 23 | `stacer --version` | `nexis --version` |
| 43 | `Open Stacer` | `Open Nexis` |
| 54 | `~/.config/stacer/stacer.log` | `~/.config/nexis/nexis.log` |

---

## 6. Documentation

### .gitignore
| Line | Current Value | Change To |
|------|--------------|-----------|
| 1 | `Stacer.pro.*` | Remove (no `Stacer.pro` file exists) or rename to `Nexis.pro.*` |

### CLAUDE.md
| Line | Reference | Change To |
|------|-----------|-----------|
| 1 | `# Stacer — Claude Code Project Instructions` | `# Nexis ...` |
| 5 | `Stacer is a Linux System Optimizer...` | `Nexis is...` |
| 41-42 | `stacer-core/`, `stacer/` directory references | Update to new dir names |
| 49 | `QuentiumYT/Stacer` | Keep as historical reference |

### README.md
Extensive rewrite needed — nearly every line references "Stacer" in:
- Title, badges, and shield URLs (lines 3, 8, 12-14)
- Description and fork attribution (line 38)
- Release links (line 51)
- Screenshot image URLs (lines 105-145)
- Contribution links and OpenCollective references (lines 154-184)

### FEATURES.md
| Line | Reference | Change To |
|------|-----------|-----------|
| 1 | `# Stacer Feature Matrix` | `# Nexis Feature Matrix` |

### BUGS.md
File path references throughout (e.g., `linux/stacer-core/...`, `shared/stacer/...`). Upstream issue links to `oguzhaninan/Stacer` should be kept as historical references. File paths need updating to match new directory names.

### FEATURE_REQUESTS.md
- Lines 19-30: Upstream `oguzhaninan/Stacer` issue links (keep as historical)
- Line 37: `lsimpsonsfdc/Stacer` reference (update URL)
- Line 39: `lsimpsonsfdc/Stacer` issue link (update URL)

---

## 7. Summary Statistics

| Category | Count |
|----------|-------|
| Files to potentially remove | 16 (old v1.0.9 screenshots) |
| Files to rename | 34 (icons, translations, desktop, icns, directories) |
| Directories to rename | 6 (source directories) |
| Source files with code references | ~24 C++ files + 15 headers |
| Build/config files to update | CMakeLists.txt + 3 Debian files |
| CI/CD files to update | 3 workflow/template YAML files |
| Documentation files to update | 6 markdown files |
| Translation files with internal references | 26 `.ts` files |
