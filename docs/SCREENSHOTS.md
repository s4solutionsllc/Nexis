# Documentation screenshots

Nexis captures its own documentation screenshots. The command opens each page, sets up the documented
state, grabs the window with `QWidget::grab()`, adds a window frame and shadow, and writes the PNG under
the file name the docs already use.

```bash
# from the repo root, after a Release build
./build/output/nexis --capture-docs .                                   # Linux
./build/output/nexis.app/Contents/MacOS/nexis --capture-docs .          # macOS
```

Run it from a normal desktop session; the window is shown while it works. The capture renders the
widgets itself, so it needs no screen-recording permission, works on Wayland, and isn't affected by other
windows covering Nexis. A full run takes about 3–4 minutes, mostly waiting for the System Cleaner scan and
for the Resources charts to fill. The process exits 0 when every shot was written and 1 if any step
failed. The failures are listed at the end of the output.

## Which platform captures what

`shared/nexis/static/doc_capture/manifest.json` lists every screenshot, with its platform:

| Platform | Files |
|---|---|
| macOS | `website/public/images/guide/*` for pages both platforms share, and `website/public/images/screenshots/*` (website gallery and README) |
| Linux | `screenshots/Nexis-01.png` … `Nexis-18.png` (AppStream / Flathub, referenced by `linux/metainfo/…metainfo.xml`), plus the Linux-only guide images `apt-repos`, `docker`, `gnome-settings*` and `boot-analysis` |

`--capture-list` prints the shots for the current platform. The `DocCaptureManifestTests` unit test
fails if the README, website guide, gallery or metainfo reference a screenshot that no manifest entry
produces.

Captured by hand: `website/public/images/guide/dashboard-availablestyles.png` (a dashboard tile's style
menu, a popup menu the runner can't open without blocking).

## Options

| Option | Default | Meaning |
|---|---|---|
| `--capture-docs <dir>` | — | Output root. Manifest paths are relative to it; use the repo root. |
| `--capture-only <patterns>` | all | Comma-separated wildcards matched against the path or file name, e.g. `Nexis-0*,docker.png`. |
| `--capture-size <W>x<H>` | `1320x840` | Main window size in logical pixels. The seeded Dashboard layout uses 8 columns, which needs about 1300 px. |
| `--capture-theme dark\|light` | `dark` | Color scheme for the run. |
| `--capture-profile fresh\|current` | `fresh` | `fresh` uses a throwaway profile (Qt test-mode paths, reset every run): default settings plus the fixed Dashboard layout in `static/doc_capture/dashboard_layout.json`, and your real settings, history and schedules are never read or written. It can run while your normal Nexis is open. `current` uses your own profile and takes the single-instance lock. |
| `--capture-no-frame` | off | Write the bare window contents without the frame and shadow. |
| `--capture-manifest <file>` | bundled | Use another manifest (same format). |
| `--capture-list` | — | Print this platform's shots and quit. |

Captures come out at the display's pixel density: on a Retina or 200%-scaled display every image is 2×.

## Before you run it

- **Hostname.** Substituted automatically: while `--capture-docs` runs, `InfoManager::getHostname()`
  returns `nexis-demo` (`Common/doc_capture_mode.h`), so the Dashboard system bar, Hardware Info and the
  health report never show the real one. Normal launches are unaffected. Any new place that renders a
  machine-identifying value should get the same treatment at its source rather than a `blur` entry.
- **Known accepted exposures.** These render real machine values and are not substituted. Check them
  by eye before committing:

  | Where | What | Why accepted |
  |---|---|---|
  | System Logs | Free-text log messages can contain the hostname, username or paths. | Arbitrary content from the OS log; can't be rewritten without changing what the page shows. Review the image. |
  | Helpers → Hosts | The `IP Address` column of `/etc/hosts`. | Normally loopback and broadcast entries; hostnames and aliases are blurred. Review for LAN addresses. |
  | Docker | Container names, ports and IDs. | The image column is blurred; the rest comes from whatever containers the capture host runs. Use a clean demo container. |
  | Dashboard / Settings / Disk Tools | Volume labels (for example `Macintosh HD`). | Stock OS labels; check a custom-named volume isn't on screen. |

  Audited and not rendered by any manifest page: local username (Processes `User` is blurred; Search
  shows none), home path (the Settings Downloads path is below the fold; the System Cleaner shortens it
  to `~`), IP and MAC addresses (Network Usage shows interface names only), and hardware serials or UUIDs.
- **Live data.** Machine-specific names are blurred automatically on both platforms: every name and
  path on Services and Startup Apps, and APT repository names (`blur`); and the name columns of
  Processes (Name, User, Process), Boot Analysis (Service), Docker (Image), Helpers → Hosts (Full
  Qualified, Aliases), the Uninstaller (Application), APT updates (Package) and Homebrew (App,
  Package) (`blurColumns`). List only columns the shot actually shows — a column on a hidden tab
  counts as missing and fails the shot. System Logs is not blurred; review it before committing. Disk Map scans `/Applications` on macOS and `/usr` on Linux, never your home
  folder.
- **Linux specifics.** Run on Ubuntu 26.04 with GNOME (the reference platform). Docker must be running
  with at least one container for `docker.png` and `Nexis-12.png`. `Nexis-04.png` needs an NVIDIA or AMD
  GPU for the GPU/VRAM columns. Boot Analysis needs `systemd-analyze`.

## Review and commit

```bash
git status --short website/public/images screenshots
```

Open the changed images, check them against the privacy points above, then commit them. If a shot
failed, re-run just that file with `--capture-only <name>.png`.

## Manifest format

```json
{ "version": 1, "shots": [
  { "file": "website/public/images/guide/search.png",   // output, relative, .png
    "page": "search",                                    // App page id ("" = stay)
    "platforms": ["macos"],                              // omit = both
    "settle": 1.5,                                       // seconds after navigation
    "steps": [ { "do": "click", "object": "btnAdvancePaneToggle" } ],
    "after": [ { "do": "click", "object": "btnAdvancePaneToggle" } ],
    "dialog": "",                                        // capture this window title instead
    "blur": ["lblServiceName"],                          // objectNames to blur (redaction)
    "blurColumns": ["Process"],                          // item-view column headers to blur
    "frame": true }
] }
```

Steps: `click` (`object` = objectName, or `text` = button label), `setText` (line edit / editable
combo), `wait` (`seconds`), `waitEnabled` (`object`, `seconds` timeout), `waitUptime` (wait until the
app has run `seconds`), `scroll` (`position` 0–1 of the page's main scroll area), `expandTrees`, `sort`
(`text` = column header, `order` asc/desc), `showPalette`, `key` (`text` = key sequence, e.g.
`Escape`), `closeDialog` (`text` = window title), `showColumn` (`text` = header of a hidden column). `blur` blurs every visible widget with the listed
object names (clipped to what's on screen) before the frame is added; `blurColumns` blurs the visible
cells of every table/tree column with that header. A shot fails if any blur target isn't visible. Page ids are the `PageSlot` ids in `app.cpp`
(`dashboard`, `resources`, `processes`, `networkUsage`, `hardwareInfo`, `bootAnalysis`, `systemLogs`,
`systemCleaner`, `diskTools`, `diskMap`, `mailCleanup`, `shredder`, `search`, `uninstaller`,
`startupApps`, `services`, `aptSourceManager` (Homebrew on macOS), `docker`, `helpers`, `gnomeSettings`,
`settings`).

When you add a guide image: add a manifest entry, run `--capture-only` for it, and run
`ctest -R DocCaptureManifest`.
