# Nexis — Feature Recommendations

> **Date:** 2026-06-10
> **Inputs:** Ubuntu 26.04 LTS "Resolute Raccoon" (released 2026-04-23: GNOME 50, kernel 7.0, systemd 259, APT 3.1), macOS 26 "Tahoe" (shipping) and macOS 27 "Golden Gate" (announced WWDC 2026-06-08), plus [docs/COMPETITIVE_LANDSCAPE.md](docs/COMPETITIVE_LANDSCAPE.md).
> **Goal:** position Nexis as the preeminent cross-platform system optimizer. Recommendations are grouped **Both platforms / Linux / macOS**, each tagged **Defensive** (a platform change Nexis must react to or it breaks/regresses) or **Offensive** (a new capability that widens the moat).
> Companion: the audit findings are in [2026-06-10-audit.md](2026-06-10-audit.md). **Note:** several audit fixes (H6 headless `--clean`, H7 Flatpak, A2 every-N-days) are prerequisites for features below — flagged inline.

---

## How to read this

Each item lists **why now** (the platform fact that motivates it), **what to build**, and a rough **effort** (S/M/L). Sources for every platform claim are in the research appendix at the end. Priority tiers at the very bottom.

The single most important strategic fact: **Ubuntu 26.04's default install now ships "Resources"** (replacing GNOME System Monitor *and* Power Statistics) **with GPU/NPU monitoring built in**, and **dropped the "Software & Updates" GUI entirely**. The in-box monitor got better — so Nexis's monitoring must go deeper than table-stakes — but Ubuntu simultaneously *removed* the GUI for managing APT repositories, PPAs, and mirrors. That repo-management vacuum on a stock 26.04 desktop is the clearest greenfield opportunity Nexis has.

---

## Both platforms

### BP1 — Duplicate & large-file finder *(Offensive, M)*
**Why now:** the single most-cited gap vs competitors — Czkawka (Linux/macOS/Windows) and CleanMyMac X both have it; Nexis has none. It's also platform-neutral (pure filesystem + hashing), so one implementation serves both OSes.
**What:** a new cleaner sub-page that finds duplicate files (size pre-filter → partial hash → full hash), large files, and empty folders, with a results table and safe-delete-to-trash. Reuse the exclusion engine and the move-to-trash path already in `CleanerService`.
**Note:** build on top of the [H9](2026-06-10-audit.md) test harness — this is destructive code and must ship with coverage.

### BP2 — App-specific cleaning profiles *(Offensive, M→L)*
**Why now:** BleachBit has 1,000+ profiles, CleanMyMac X has deep macOS-specific cleaners; Nexis has 6 general categories — its biggest cleaning-depth weakness. A small data-driven profile format (declarative JSON/INI of glob patterns + safety class) lets the community contribute profiles without C++ changes.
**What:** a profile schema + loader, seeded with the top 20–30 apps per platform (browsers, IDEs, Docker, package-manager caches). Each profile declares paths, an age policy, and whether it's "safe" vs "aggressive."

### BP3 — Disk space visualizer (sunburst/treemap) *(Offensive, M)*
**Why now:** today Nexis only *launches* external tools (Filelight/Baobab); DaisyDisk's sunburst is a flagship paid-macOS feature and a recurring "why isn't this built in" request. A built-in `QGraphicsView` treemap is cross-platform and pairs naturally with the cleaner.
**What:** recursive size scan on a worker thread (reuse the async patterns), rendered as an interactive treemap with drill-down and "reveal in file manager"/"move to trash."

### BP4 — Software updater surfacing *(Offensive, M)*
**Why now:** CleanMyMac X has it; on Linux it maps to `apt`/`snap`/`flatpak` upgrades, on macOS to `brew outdated` + `softwareupdate`. Nexis already brokers these package managers — exposing "what's upgradable" is incremental.
**What:** an "Updates" view aggregating outdated packages across the platform's managers with one-click upgrade (reusing existing privilege paths).

### BP5 — Health-report export & scheduled reports *(Offensive, S)*
**Why now:** differentiator vs every single-purpose competitor; trivially cross-platform; pairs with the existing dashboard + maintenance wizard.
**What:** "Export system health report" (Markdown/PDF) summarizing CPU/mem/disk/battery/SMART/thermal + cleanable space, optionally on the existing schedule.

---

## Linux

### LX1 — deb822 APT source editor + the "Software & Updates" vacuum *(Defensive + Offensive, L) — flagship opportunity*
**Why now:** **APT 3.1 made deb822 `.sources` the default format** and **removed `apt-key`** (keys must live in `/etc/apt/keyrings/` or `/usr/share/keyrings/` with `Signed-By:`). Simultaneously, **Ubuntu 26.04 dropped the "Software & Updates" GUI from the default install** — a stock desktop now has *no graphical way* to manage PPAs, mirrors, or repo components. Nexis's APT-sources page must be updated to parse/write deb822 and handle `Signed-By` keyrings (the old one-line `.list` + `apt-key` model is deprecated/removed) — and once it does, **it becomes the only repo-management GUI on a stock 26.04 desktop.**
**What:** (1) read/write deb822 `ubuntu.sources`; (2) manage `Signed-By` keyrings without `apt-key`; (3) PPA add/remove via the retained `add-apt-repository` CLI; (4) component/mirror toggles. This is both a correctness fix (the current page will mis-handle 26.04 sources) and the highest-leverage new-user acquisition feature available.

### LX2 — APT history: undo / rollback / why *(Offensive, M)*
**Why now:** **APT 3.1 added dnf-style transaction history** — `apt history-list/info/undo/redo/rollback` and `apt why`/`apt why-not`. No GUI surfaces these yet.
**What:** a transaction-history view in the uninstaller/sources area with one-click undo of the last operation and a "why is this installed?" dependency explainer. A genuinely novel GUI feature on Ubuntu 26.04.

### LX3 — Wayland-only readiness audit *(Defensive, S→M) — must-do*
**Why now:** **GNOME 50 removed the X11 session entirely** (Mutter/Shell/GDM); 26.04 GNOME is Wayland-only (XWayland remains for app compatibility). Any X11-specific code path in Nexis (screenshot capture, window/screen enumeration via X11, `XTest`) is dead on a stock 26.04 GNOME session.
**What:** audit for X11 assumptions; confirm the Qt6 app runs as a native Wayland client; ensure the screenshot test harness and any window-geometry logic work under Wayland/XWayland. Low feature value, high "doesn't silently break" value.

### LX4 — cgroup v2 / systemd-oomd observability *(Defensive + Offensive, M)*
**Why now:** **systemd 259 removed cgroup v1 entirely** — 26.04 refuses to run on v1 hosts, so any per-process accounting that assumes v1 paths breaks; and **systemd-oomd now exposes `OOMKills`/`ManagedOOMKills` properties** per unit. Kernel 7.0 also added a generic FS-error-reporting channel.
**What:** ensure process/dashboard accounting reads cgroup v2 paths only (defensive), then surface OOM-kill counts and recent OOM events as a new dashboard signal and a "what got killed and why" panel (offensive — the in-box Resources app doesn't show this).

### LX5 — Services page: SysV deprecation + soft-reboot / run0 *(Defensive + Offensive, M)*
**Why now:** **systemd 260 will remove SysV init-script compatibility** (259 is the last with it), so the services page must not assume `/etc/init.d` generators persist. systemd also gained `soft-reboot` (userspace-only restart) and `run0` (the `sudo` alternative, now with `--empower`); 26.04 ships `sudo-rs` as the default sudo.
**What:** drop SysV assumptions; optionally add a "soft-reboot" action (faster than full reboot after updates) and validate elevation against `run0`/`sudo-rs` environments.

### LX6 — Battery charge-threshold control *(Offensive, S→M)*
**Why now:** GNOME 50's Power panel added firmware charge modes ("Maximize Charge" / "Preserve Battery Health"), but the stock UI is limited and has a **known bug where the mode reverts on some Dell XPS**. Custom thresholds live in `/sys/class/power_supply/BATx/charge_control_end_threshold`. Nexis already reads battery health.
**What:** expose a charge-threshold slider/control writing the sysfs threshold (via the existing pkexec path), filling a gap the stock GNOME UI handles poorly.

### LX7 — New hwmon/WMI sensor surfaces *(Offensive, S)*
**Why now:** kernel 7.0 added vendor WMI sensors — **ASUS WMI** (fan/backlight/kbd hotkeys), **HP WMI manual fan control** (Victus), **Lenovo WMI extra HWMON sensors** (Legion). These are new readable sysfs/hwmon nodes.
**What:** extend the hardware-info/thermal pages to enumerate and display these vendor sensors when present (gaming-laptop audience overlaps heavily with "system optimizer" users).

### LX8 — Removable-media path fix *(Defensive, S) — quiet correctness bug*
**Why now:** GNOME 50 **mounts removable media under `/run/media` instead of `/media`.** Any cleaner/disk code scanning `/media` mount points will miss removable drives on 26.04.
**What:** update mount-point scanning to handle `/run/media` (keep `/media` for older distros).

### LX9 — Snap-packaging reality check *(Defensive, M)*
**Why now:** 26.04 ships **AppArmor/snapd permission prompting** (opt-in but expanding); a strict snap with Nexis's broad system access (process kill, file cleaning, service control) would collide with prompting rules — and there are known 26.04.0 kernel-panic bugs with prompting on Downloads access. The **App Center now manages debs**, overlapping the uninstaller. Packaging Nexis as a **deb avoids snapd prompting friction entirely.**
**What:** confirm the deb is the primary Ubuntu distribution channel; if a snap is ever pursued, design for prompting. (Relatedly, audit [H7](2026-06-10-audit.md) settles Flatpak's fate.)

---

## macOS

### MX1 — macOS 27 launchd quarantine-xattr handling *(Defensive, S) — will break startup items & scheduled cleaning*
**Why now:** **macOS 27 (Golden Gate) refuses to load launchd plists carrying the `com.apple.quarantine` xattr.** Nexis writes launchd plists for startup-items management *and* for scheduled cleaning agents; any such plist that inherits quarantine will silently fail to load on macOS 27.
**What:** strip the quarantine xattr from every plist Nexis writes (and document the requirement). This is small but the failure is silent and high-impact on the two macOS scheduling features.

### MX2 — macOS 27 cross-team container access *(Defensive, M) — cache cleaning regression*
**Why now:** **macOS 27 denies cross-team app-container access by default with no prompt** — scanning/deleting other apps' `~/Library/Containers` data silently fails unless the user enables access in Privacy & Security. This directly degrades cache cleaning on macOS 27.
**What:** detect denied container access, and instead of silently skipping, surface a clear "grant Full Disk Access / enable in Privacy & Security" prompt with a deep link. Verify actual behavior against the macOS 27 beta before GA.

### MX3 — Background Task Management (btm) startup-items integration *(Offensive, M)*
**Why now:** `SMAppService` is the blessed API; `sfltool dumpbtm` is the diagnostic for login items / background tasks, and Tahoe 26.4 has a **known bug where `backgroundtaskmanagementd` pegs 400 % CPU** and login items fail. Nexis's startup-apps page can be the best GUI for this messy area.
**What:** parse `sfltool dumpbtm` to show *all* background task manager items (not just user login items), flag orphaned/duplicate entries, and offer `sfltool resetbtm` as a repair action — directly addressing a real, current Tahoe pain point. Ensure the scanner doesn't choke on the new `/System/Library/LaunchAngels` directory.

### MX4 — Menu-bar live monitor (iStat-Menus-style) *(Offensive, L)*
**Why now:** the most-cited macOS monitoring gap vs iStat Menus (always-on menu-bar CPU/mem/net/temp). Tahoe added a **Menu Bar section in System Settings** that OS-manages per-item visibility, so a new `NSStatusItem` integrates cleanly with the platform.
**What:** an optional menu-bar agent showing live CPU/mem/network/thermal, clicking through to the dashboard. Pairs with Nexis's existing kiosk mode as the "always visible" story. Significant effort (a separate status-item lifecycle), so tier-2.

### MX5 — macOS deep-maintenance tasks (OnyX-style) *(Offensive, M)*
**Why now:** OnyX's signature features — Spotlight index rebuild, disk-structure verification, flushing system caches, hidden Finder/Dock/Safari settings — are absent in Nexis and are exactly what macOS power users reach for. They map to `mdutil`, `diskutil verifyVolume`, and `defaults`.
**What:** a "Maintenance" panel exposing safe, well-labeled versions of these (Spotlight reindex, verify disk, rebuild Launch Services, toggle common hidden defaults), reusing the maintenance-wizard UI pattern.

### MX6 — App-uninstall leftover thoroughness (AppCleaner-parity) *(Offensive, M)*
**Why now:** AppCleaner is noted as likely more thorough than Nexis at finding an app's scattered support files. With the AppleScript-injection fix ([S1](2026-06-10-audit.md)) landing in the uninstaller anyway, it's a natural time to deepen it.
**What:** when uninstalling a `.app`, scan the standard leftover locations (`~/Library/{Application Support,Caches,Preferences,Logs,Containers,Saved Application State}`, LaunchAgents) for matching bundle-id artifacts and offer them for removal.

### MX7 — Intel/Apple-silicon & Qt-version strategy *(Defensive, S — planning)*
**Why now:** **macOS 26 is the last to support Intel; macOS 27 is Apple-silicon-only** and Rosetta is being phased out (through macOS 27 only). **Qt supports macOS 26 from Qt 6.10** (backported to 6.8 LTS / 6.5 patches); Liquid Glass forced rendering changes (a compatibility mode preserves the pre-Tahoe look). Xcode 26 no longer bundles the Metal toolchain by default.
**What:** decide the Intel-build sunset timeline (Intel matters only for ≤26 users); pin the macOS build to Qt ≥6.8-latest-patch (ideally 6.10/6.11); test the UI under Tahoe's Liquid Glass and macOS 27's transparency slider; keep the notarized `.dmg` channel (Tahoe skips first-run XProtect scan for notarized apps) and **avoid `.pkg`** until the 26.3 Gatekeeper-rejection issue clears.

---

## Cross-cutting / strategic

- **Don't chase remote/web monitoring or Windows.** The competitive doc positions Nexis as a local, native, cross-platform (Linux+macOS) tool; Glances/Netdata/Cockpit own remote, and Windows would triple the platform-matrix cost the audit already flags ([A3](2026-06-10-audit.md)). Stay in lane.
- **Lead the Ubuntu 26.04 narrative.** "The repo-management GUI Ubuntu removed, plus OOM/GPU observability the stock Resources app doesn't show" (LX1+LX2+LX4) is a launch story for the next release window. The 4–6 week cadence in the SOP fits a "Nexis 2.4 — Ubuntu 26.04 ready" themed release.
- **Use the audit fixes as feature enablers.** Headless `--clean` ([H6](2026-06-10-audit.md)) unlocks reliable scheduled cleaning (and a future CLI); the `RepositoryTool` refactor ([A4](2026-06-10-audit.md)) is the natural home for LX1/LX2; the destructive-path test harness ([H9](2026-06-10-audit.md)) is a prerequisite for BP1/BP2.

---

## Suggested priority tiers

**Tier 1 — defensive must-dos (ship before/with 26.04 + macOS 27 support):**
LX1 (deb822/keyrings — also a correctness bug), LX3 (Wayland readiness), LX8 (`/run/media`), MX1 (quarantine xattr), MX2 (container access), MX7 (Qt/Intel strategy).

**Tier 2 — high-leverage offensive (the moat-wideners):**
LX1's "vacuum" framing + LX2 (APT history GUI), BP1 (duplicate finder), BP3 (disk visualizer), MX3 (btm startup items), LX4 (OOM observability).

**Tier 3 — depth & polish:**
BP2 (cleaning profiles), BP4 (updater), BP5 (health report), LX6 (charge thresholds), LX7 (vendor sensors), MX5 (OnyX-style maintenance), MX6 (uninstall thoroughness).

**Tier 4 — larger bets:**
MX4 (menu-bar monitor), LX5 (soft-reboot/run0 services rework).

---

## Research appendix — platform facts & sources

### Ubuntu 26.04 LTS "Resolute Raccoon" (2026-04-23)
- **GNOME 50 "Tokyo":** Wayland-only (X11 session removed from Mutter/Shell/GDM; XWayland retained); fractional scaling/VRR default; new **Resources** app replaces System Monitor + Power Statistics with **GPU/NPU monitoring**; removable media moves to **`/run/media`**; Power panel adds firmware charge modes. Sources: [release-notes 26.04](https://documentation.ubuntu.com/release-notes/26.04/), [GNOME 50 (Register)](https://www.theregister.com/2026/03/19/gnome_50/), [9to5linux GNOME 50](https://9to5linux.com/gnome-50-tokyo-desktop-environment-officially-released-this-is-whats-new).
- **Kernel 7.0:** sched_ext stable, Rust non-experimental, kdump default; new vendor WMI hwmon sensors (ASUS/HP/Lenovo); generic FS error reporting. Sources: [kernelnewbies 7.0](https://kernelnewbies.org/Linux_7.0), [9to5linux kernel 7.0](https://9to5linux.com/linux-kernel-7-0-officially-released-this-is-whats-new).
- **systemd 259:** cgroup v1 removed (won't run on v1 hosts); last release with SysV compat (gone in 260); `run0 --empower`; `soft-reboot`; oomd `OOMKills`/`ManagedOOMKills`. Sources: [systemd v259 release](https://github.com/systemd/systemd/releases/tag/v259), [LWN 1051235](https://lwn.net/Articles/1051235/).
- **APT 3.1:** solver3 default; **`apt-key` removed** (Signed-By keyrings); **deb822 `.sources` default**; `apt history` undo/rollback/redo; `apt why`/`why-not`. Sources: [linuxconfig APT 3.1](https://linuxconfig.org/what-is-new-in-apt-3-1-on-ubuntu-26-04), [LWN 1017315](https://lwn.net/Articles/1017315/).
- **Software & Updates GUI removed from default install** (repo-management vacuum); **App Center now manages debs**; AppArmor/snapd permission prompting expanding; new **Security Center**. Sources: [omgubuntu S&U removal](https://www.omgubuntu.co.uk/2026/02/ubuntu-26-04-drops-software-and-updates-gui), [Launchpad #2140527](https://bugs.launchpad.net/ubuntu/+source/software-properties/+bug/2140527), [itsfoss deb in App Center](https://itsfoss.com/news/ubuntu-26-04-deb-in-app-center/), [permissions prompting deep-dive](https://discourse.ubuntu.com/t/permissions-prompting-a-deep-dive/81785).
- **sudo-rs default**; **battery charge-mode UI bug on some Dell XPS**. Sources: [canonical 26.04 blog](https://canonical.com/blog/canonical-releases-ubuntu-26-04-lts-resolute-raccoon), [charge-options UI bug](https://discourse.ubuntu.com/t/ubuntu-26-04-lts-battery-charging-options-user-interface-bug/81113).

### macOS 26 "Tahoe" (shipping) / macOS 27 "Golden Gate" (WWDC 2026-06-08)
- **macOS 26:** last Intel-supporting release; Menu Bar section in System Settings (OS-managed `NSStatusItem` visibility); skips first-run XProtect scan for notarized apps; Tahoe 26.4 `backgroundtaskmanagementd` 400% CPU bug; new `/System/Library/LaunchAngels`. Sources: [Macworld Tahoe](https://www.macworld.com/article/2644146/macos-26-features-latest-update-release-date-beta.html), [Eclectic Light XProtect](https://eclecticlight.co/2025/09/30/do-apps-launch-faster-in-macos-tahoe/), [Eclectic Light LaunchAngels](https://eclecticlight.co/2025/10/03/welcome-to-tahoes-launch-angels/), [MacRumors btm bug](https://forums.macrumors.com/threads/problem-with-background-tasks-login-items-after-upgrade-to-macos-26-4.2480630/).
- **macOS 27:** Apple-silicon-only (Intel dropped); **launchd refuses quarantine-xattr plists**; **cross-team container access denied by default, no prompt**; TCC.db locked down (API-only checks). Sources: [TechRadar macOS 27](https://www.techradar.com/computing/mac-os/macos-27-golden-gate-announced-at-wwdc-2026-heres-everything-you-need-to-know), [Michael Tsai roundup](https://mjtsai.com/blog/2026/06/09/macos-golden-gate-27-announced/), [Apple macOS 27 release notes](https://developer.apple.com/documentation/macos-release-notes/macos-27-release-notes).
- **Qt:** macOS 26 supported from **Qt 6.10** (backported to 6.8 LTS/6.5); Liquid Glass rendering changes + compatibility mode; Xcode 26 drops bundled Metal toolchain. Sources: [Qt on macOS 26 Tahoe](https://www.qt.io/blog/qt-on-macos-26-tahoe), [Qt supported platforms](https://doc.qt.io/qt-6/supported-platforms.html).
- **Gatekeeper:** tightened quarantine enforcement; active 26.3 reports of notarized `.pkg` rejections — prefer notarized `.dmg`. Source: [Apple dev forums — Gatekeeper](https://developer.apple.com/forums/tags/gatekeeper).

### Competitor gaps (from [docs/COMPETITIVE_LANDSCAPE.md](docs/COMPETITIVE_LANDSCAPE.md), Feb 2026)
Duplicate detection (Czkawka, CleanMyMac X); app-specific cleaning profiles (BleachBit 1,000+); disk visualizer (DaisyDisk); software updater + malware scan (CleanMyMac X); menu-bar monitoring/per-app bandwidth/fan control (iStat Menus); deep maintenance tasks (OnyX); uninstall leftover thoroughness (AppCleaner); advanced package management (Synaptic). Nexis deliberately cedes remote/web monitoring (Glances/Netdata/Cockpit) and Windows.
