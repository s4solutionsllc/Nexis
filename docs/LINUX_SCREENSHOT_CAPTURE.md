# Linux screenshot capture checklist

The macOS set (website guide and GitHub/website hero screenshots) was refreshed on 2026-11-20 from a clean
Release build. This checklist covers the images that must come from Linux, because they show Linux-only
pages or are published to Linux app stores.

## Setup (once)

1. Use Ubuntu 26.04 LTS, GNOME, Wayland session (the reference platform for Linux baselines).
2. Build Nexis from the same commit as the macOS set:
   `rm -rf build && cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j$(nproc)`
3. Back up `~/.config/nexis/settings.ini`, then launch `build/output/nexis --nosplash`.
4. In Nexis → Settings → Appearance: **Color Scheme = Dark**, **Font = Inter (Recommended)**.
5. Resize the window to about **1280 × 800** and keep it the same size for every shot.
6. Expand the sidebar (not collapsed). Leave Diagnose/Clean collapsed and Manage/Tools expanded,
   unless the page you are capturing is inside a collapsed section.
7. Keep the shots clean of personal data:
   - Set a neutral hostname for the session (for example `nexis-demo`). The Dashboard system bar and
     Hardware Info show it.
   - Don't show your home folder or your project names. For Disk Map, scan `/usr`. For Search, keep the
     empty state or search `/usr/share`.
   - Hide private repositories, internal services and other people's hosts.
8. Capture **the window only, with its shadow**: GNOME Screenshot (Print Screen → Window), or
   `gnome-screenshot -w -f <file>.png`. Save as PNG.
9. Wait for live data to fill before capturing. The Resources charts need about 60 s, and System Cleaner
   needs its scan to finish.

## A. AppStream / Flathub set: `screenshots/Nexis-NN.png`

`linux/metainfo/io.github.s4solutionsllc.Nexis.metainfo.xml` uses 01–06. The existing files don't match
those captions (for example, 03 currently shows Startup Apps). Capture each one to match its caption.
07–18 aren't referenced anywhere today; refresh them so the set stays complete.

| File | Page / state to capture | AppStream caption |
|---|---|---|
| `Nexis-01.png` | Dashboard, default layout, live values | Dashboard with live system metrics |
| `Nexis-02.png` | Hardware Info, top of page | Hardware information overview |
| `Nexis-03.png` | System Cleaner after a scan, with sizes shown on the category cards | System Cleaner with category cards |
| `Nexis-04.png` | Processes, sorted by %CPU, with GPU/VRAM columns visible (needs an NVIDIA/AMD GPU) | Processes page with GPU and VRAM columns |
| `Nexis-05.png` | Resources → Live, after 60 s (2-column grid at ≥1280 px wide) | Resources page with live charts |
| `Nexis-06.png` | APT Repository Manager | APT Repository Manager |
| `Nexis-07.png` | Services (systemd list) | — |
| `Nexis-08.png` | Startup Apps | — |
| `Nexis-09.png` | Uninstaller (Packages tab, groups expanded) | — |
| `Nexis-10.png` | File Search with Advanced Search open | — |
| `Nexis-11.png` | Helpers → Host Manage | — |
| `Nexis-12.png` | Docker with the daemon running and at least one container | — |
| `Nexis-13.png` | GNOME Settings → Appearance | — |
| `Nexis-14.png` | GNOME Settings → Window Manager | — |
| `Nexis-15.png` | GNOME Settings → Mouse/Touchpad | — |
| `Nexis-16.png` | Boot Analysis with per-service timings | — |
| `Nexis-17.png` | Settings (top of page) | — |
| `Nexis-18.png` | Feedback dialog only (window capture of the dialog) | — |

Do not recapture `screenshots/header.png` or `header-old.png`. They are artwork, not screenshots.

## B. Website guide: `website/public/images/guide/`

These guide pages show Linux-only features. Use the same file names.

| File | Page / state | Used in |
|---|---|---|
| `apt-repos.png` | APT Repository Manager | guide 12, README |
| `docker.png` | Docker, daemon running, containers and images listed | guide 13 |
| `gnome-settings.png` | GNOME Settings → Appearance | guide 14, README |
| `gnome-settings-windowmanager.png` | GNOME Settings → Window Manager | README |
| `gnome-settings-mouse.png` | GNOME Settings → Mouse/Touchpad | README |
| `boot-analysis.png` | Boot Analysis with per-service timing (macOS only shows uptime) | guide 21 |

## C. Not refreshed on macOS (either platform)

| File | What to capture |
|---|---|
| `website/public/images/guide/dashboard-availablestyles.png` | Dashboard → Customize Layout (Ctrl+E) → a tile's ✎ style menu open. Crop to the menu only (about 420 px wide). |

## After capturing

1. Copy the files over the existing ones (same names), then run `git status` and check that only the
   expected PNGs changed.
2. Restore your backed-up `settings.ini`.
3. Commit them with the macOS refresh.
