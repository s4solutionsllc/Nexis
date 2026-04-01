
<p align="center">
    <img src="https://raw.githubusercontent.com/s4solutionsllc/Nexis/native/screenshots/header.png" width="800">
</p>

<p align="center">
  <b>Linux & macOS System Optimizer and Monitor</b><br>
  <sub>The only free, open-source, cross-platform all-in-one system optimizer, monitor, and manager</sub>
</p>

<p align="center">
  <a href="https://github.com/s4solutionsllc/Nexis/releases/latest"><img src="https://img.shields.io/github/v/release/s4solutionsllc/Nexis?label=latest%20release" alt="Latest Release"></a>
  <a href="https://github.com/s4solutionsllc/Nexis/actions/workflows/build.yml"><img src="https://github.com/s4solutionsllc/Nexis/actions/workflows/build.yml/badge.svg?branch=native" alt="Build Status"></a>
  <a href="https://github.com/s4solutionsllc/Nexis/blob/native/LICENSE"><img src="https://img.shields.io/github/license/s4solutionsllc/Nexis" alt="License: GPL v3"></a>
  <img src="https://img.shields.io/badge/Qt-6-41cd52?logo=qt" alt="Qt 6">
  <img src="https://img.shields.io/badge/C%2B%2B-17-blue?logo=cplusplus" alt="C++17">
  <img src="https://img.shields.io/badge/platforms-Linux%20%7C%20macOS-lightgrey" alt="Platforms: Linux | macOS">
  <img src="https://img.shields.io/github/downloads/s4solutionsllc/Nexis/total" alt="Downloads">
  <a href="https://launchpad.net/~s4solutionsllc/+archive/ubuntu/nexis"><img src="https://img.shields.io/badge/PPA-s4solutionsllc%2Fnexis-E95420?logo=ubuntu" alt="PPA: s4solutionsllc/nexis"></a>
    <!--<a href="https://crowdin.com/project/nexis"><img src="https://badges.crowdin.net/nexis/localized.svg" alt="Crowdin"></a>-->
</p>

## Features

### Monitor
- **Dashboard** -- Real-time CPU, memory, disk, GPU, and network monitoring with customizable tile layouts, a system health score, and a one-click maintenance wizard
- **Hardware Info** -- Detailed system, processor, graphics, memory, battery, and disk information at a glance
- **Resource Monitor** -- Historical charts for CPU, memory, GPU, network, and disk I/O
- **Kiosk Mode** -- F11 fullscreen dashboard-only mode for dedicated monitoring displays
- **GPU Monitoring** -- GPU utilization tracking for NVIDIA, AMD, and Intel (Linux); Apple Silicon (macOS)
- **Battery & Disk Health** -- Battery cycle count and capacity degradation, SMART disk health (NVMe + SATA)

### Manage
- **System Cleaner** -- Remove package caches, crash reports, application logs, app caches, and unused Flatpak runtimes
- **Scheduled Cleaning** -- Automated background cleaning via systemd, launchd, or cron with exclusion rules
- **Disk Tools** -- Find large/old files and scan for duplicates across directories
- **File Search** -- Search files by name, extension, size, and date with advanced filters
- **Process Manager** -- View, sort, filter, and manage running processes
- **Service Manager** -- Start, stop, and toggle system services (systemd / launchd)
- **Startup Apps** -- Manage auto-start applications with configurable delay
- **Package Uninstaller** -- Uninstall packages via APT, DNF, Pacman, Snap (Linux) or Homebrew (macOS)

### System
- **Docker Management** -- GUI for managing Docker images, containers, and volumes
- **System Logs** -- View, filter, and search journald/syslog entries by severity
- **Hosts File Editor** -- Manage `/etc/hosts` entries with DNS cache flushing
- **Network Diagnostics** -- Ping, traceroute, DNS lookup, open ports viewer, and firewall management
- **Power Profiles** -- Switch between power-saver, balanced, and performance modes (Linux); Spotlight rebuild, disk verify, and LaunchServices rebuild (macOS)
- **APT Repository Manager / Homebrew Taps** -- Manage package repositories with health checks, diagnostics, and one-click repair
- **Desktop Settings** -- Adjust GNOME (Linux) or macOS desktop preferences
- **Command Palette** -- Ctrl+K quick-navigation to any page or action
- **Theme Support** -- Dark, light, and system-auto color schemes
- **Internationalization** -- 34 languages supported

## How Nexis Compares

| | **Nexis** | Stacer | CleanMyMac X | BleachBit |
|---|:---:|:---:|:---:|:---:|
| **Real-time monitoring** | :white_check_mark: | :white_check_mark: | Partial | :x: |
| **System cleaning** | :white_check_mark: | :white_check_mark: | :white_check_mark: | :white_check_mark: |
| **Process/service management** | :white_check_mark: | :white_check_mark: | :x: | :x: |
| **Package management** | :white_check_mark: | :white_check_mark: | :white_check_mark: | :x: |
| **GPU monitoring** | :white_check_mark: | :x: | :x: | :x: |
| **Hardware info panel** | :white_check_mark: | :x: | :x: | :x: |
| **Battery & disk health** | :white_check_mark: | :x: | :x: | :x: |
| **Docker management** | :white_check_mark: | :x: | :x: | :x: |
| **System log viewer** | :white_check_mark: | :x: | :x: | :x: |
| **Disk analysis & duplicates** | :white_check_mark: | :x: | :white_check_mark: | :x: |
| **Network diagnostics** | :white_check_mark: | :x: | :x: | :x: |
| **Scheduled cleaning** | :white_check_mark: | :x: | :white_check_mark: | :x: |
| **Kiosk mode** | :white_check_mark: | :x: | :x: | :x: |
| **Linux support** | :white_check_mark: | :white_check_mark: | :x: | :white_check_mark: |
| **macOS support** | :white_check_mark: | :x: | :white_check_mark: | :x: |
| **Actively maintained** | :white_check_mark: | :x: (since 2020) | :white_check_mark: | :white_check_mark: |
| **Open source** | :white_check_mark: | :white_check_mark: | :x: | :white_check_mark: |
| **Price** | **Free** | Free | ~$40/year | Free |

## Background

Nexis began as a fork of [Stacer](https://github.com/oguzhaninan/Stacer), a popular Linux system optimizer created by oguzhaninan. After the original project went inactive in 2020, development continued here -- porting to Qt 6 and C++17, adding native macOS support, GPU monitoring, a hardware info panel, kiosk mode, and fixing 38+ bugs inherited from the upstream codebase. As the feature-set diverged, the project was rebranded as Nexis to reflect that it had become something new. Stacer laid the foundation; Nexis is where it goes from here.

## Downloads

Install via [PPA](#install-via-ppa-ubuntu) for automatic updates, or download pre-built binaries from the [Releases page](https://github.com/s4solutionsllc/Nexis/releases/latest):

- **Linux x86_64**: `.deb` package, `.AppImage` portable, standalone binary
- **Linux ARM64** (aarch64): `.deb` package, `.AppImage` portable, standalone binary
- **macOS Apple Silicon**: `.dmg` disk image

### Install via PPA (Ubuntu)

```bash
sudo add-apt-repository ppa:s4solutionsllc/nexis
sudo apt update
sudo apt install nexis
```

Supports Ubuntu 22.04 (Jammy), 24.04 (Noble), and 25.04 (Plucky) on both x86_64 and ARM64. Updates are delivered automatically via `apt upgrade`.

## Screenshots

<p align="center">
    <img src="https://raw.githubusercontent.com/s4solutionsllc/Nexis/native/website/public/images/screenshots/dashboard.png" width="700">
    <br><em>Dashboard -- real-time system monitoring</em>
</p>

<p align="center">
    <img src="https://raw.githubusercontent.com/s4solutionsllc/Nexis/native/website/public/images/screenshots/system-cleaner.png" width="700">
    <br><em>System Cleaner -- remove caches, logs, and crash reports</em>
</p>

<p align="center">
    <img src="https://raw.githubusercontent.com/s4solutionsllc/Nexis/native/website/public/images/screenshots/resource-monitor.png" width="700">
    <br><em>Resources -- historical CPU, memory, GPU, network, and disk I/O charts</em>
</p>

<details>
<summary><strong>View all screenshots</strong></summary>
<br>

| Page | Screenshot |
|---|---|
| Hardware Info | <img src="https://raw.githubusercontent.com/s4solutionsllc/Nexis/native/website/public/images/screenshots/hardware-info.png" width="500"> |
| Startup Apps | <img src="https://raw.githubusercontent.com/s4solutionsllc/Nexis/native/website/public/images/guide/startup-apps.png" width="500"> |
| Search | <img src="https://raw.githubusercontent.com/s4solutionsllc/Nexis/native/website/public/images/guide/search.png" width="500"> |
| Services | <img src="https://raw.githubusercontent.com/s4solutionsllc/Nexis/native/website/public/images/guide/services.png" width="500"> |
| Processes | <img src="https://raw.githubusercontent.com/s4solutionsllc/Nexis/native/website/public/images/guide/processes.png" width="500"> |
| Uninstaller | <img src="https://raw.githubusercontent.com/s4solutionsllc/Nexis/native/website/public/images/guide/uninstaller.png" width="500"> |
| System Logs | *screenshot coming soon* |
| Resources | <img src="https://raw.githubusercontent.com/s4solutionsllc/Nexis/native/website/public/images/guide/resources-overview.png" width="500"> |
| Helpers | <img src="https://raw.githubusercontent.com/s4solutionsllc/Nexis/native/website/public/images/guide/helpers-hosts.png" width="500"> |
| APT Repository Manager | <img src="https://raw.githubusercontent.com/s4solutionsllc/Nexis/native/website/public/images/guide/apt-repos.png" width="500"> |
| GNOME Settings | <img src="https://raw.githubusercontent.com/s4solutionsllc/Nexis/native/website/public/images/guide/gnome-settings.png" width="500"> |
| GNOME Settings (Window Manager) | <img src="https://raw.githubusercontent.com/s4solutionsllc/Nexis/native/website/public/images/guide/gnome-settings-windowmanager.png" width="500"> |
| GNOME Settings (Mouse/Touchpad) | <img src="https://raw.githubusercontent.com/s4solutionsllc/Nexis/native/website/public/images/guide/gnome-settings-mouse.png" width="500"> |
| Settings | <img src="https://raw.githubusercontent.com/s4solutionsllc/Nexis/native/website/public/images/guide/settings.png" width="500"> |
| Feedback | <img src="https://raw.githubusercontent.com/s4solutionsllc/Nexis/native/website/public/images/guide/feedback.png" width="500"> |

</details>

## Building from Source

### Prerequisites

#### Linux

Install Qt 6 development libraries:

**Ubuntu / Debian:**
```bash
sudo apt install cmake g++ qt6-base-dev qt6-charts-dev qt6-svg-dev \
  qt6-tools-dev-tools qt6-l10n-tools
```

**Fedora / RHEL:**
```bash
sudo dnf install cmake gcc-c++ qt6-qtbase-devel qt6-qtcharts-devel \
  qt6-qtsvg-devel qt6-linguist
```

**Arch Linux:**
```bash
sudo pacman -S cmake qt6-base qt6-charts qt6-svg qt6-tools
```

> **Note:** On minimal or headless environments you may also need `libgl1-mesa-dev` (Debian/Ubuntu) or equivalent OpenGL headers.

#### macOS

```bash
brew install qt@6 cmake
```

After installing, ensure Qt is in your path:
```bash
export PATH="$(brew --prefix qt@6)/bin:$PATH"
```

### Build

```bash
mkdir -p build && cd build
cmake ..
make -j$(nproc 2>/dev/null || sysctl -n hw.ncpu)
```

## Development

Nexis is co-authored by [Claude Code](https://claude.ai/claude-code), Anthropic's AI coding agent. Claude Code contributes to architecture decisions, feature implementation, bug fixes, CI/CD pipelines, and release engineering -- working alongside the human maintainer as a pair-programming partner.

## Translations

Nexis supports 34 languages. Translations are managed via [Crowdin](https://crowdin.com/project/nexis) -- no coding required to contribute. Sign up, pick a language, and start translating in your browser.

New translations are automatically synced back to this repository via pull request.

### Maintainer Setup

To enable the Crowdin sync pipeline for a new fork:

1. Create a project at [crowdin.com](https://crowdin.com) (free for open source).
2. Add two repository secrets in GitHub Settings > Secrets:
   - `CROWDIN_PROJECT_ID` -- numeric project ID from the Crowdin dashboard.
   - `CROWDIN_PERSONAL_TOKEN` -- personal access token from Crowdin account settings.
3. Seed the project with existing translations (one-time):
   ```bash
   brew install crowdin                # macOS
   export CROWDIN_PROJECT_ID=<id>
   export CROWDIN_PERSONAL_TOKEN=<token>
   crowdin upload sources --config crowdin.yml
   crowdin upload translations --config crowdin.yml --auto-approve-imported
   ```
4. The `crowdin-sync.yml` GitHub Action handles ongoing sync automatically.

## Contributing

Bug reports and feature requests are welcome! Please [open an issue](https://github.com/s4solutionsllc/Nexis/issues) or submit a pull request. See [CHANGELOG.md](CHANGELOG.md) for release history.
