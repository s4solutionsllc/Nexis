
<p align="center">
    <img src="https://raw.githubusercontent.com/lsimpsonsfdc/Nexis/native/screenshots/header.png" width="800">
</p>

<p align="center">
  <b>Linux & macOS System Optimizer and Monitor</b><br>
  <sub>Actively maintained Qt6 fork of <a href="https://github.com/oguzhaninan/Stacer">oguzhaninan/Stacer</a>, rebranded as Nexis</sub>
</p>

<p align="center">
  <a href="https://github.com/lsimpsonsfdc/Nexis/releases/latest"><img src="https://img.shields.io/github/v/release/lsimpsonsfdc/Nexis?label=latest%20release" alt="Latest Release"></a>
  <a href="https://github.com/lsimpsonsfdc/Nexis/actions/workflows/build.yml"><img src="https://github.com/lsimpsonsfdc/Nexis/actions/workflows/build.yml/badge.svg?branch=native" alt="Build Status"></a>
  <a href="https://github.com/lsimpsonsfdc/Nexis/blob/native/LICENSE"><img src="https://img.shields.io/github/license/lsimpsonsfdc/Nexis" alt="License: GPL v3"></a>
  <img src="https://img.shields.io/badge/Qt-6-41cd52?logo=qt" alt="Qt 6">
  <img src="https://img.shields.io/badge/C%2B%2B-17-blue?logo=cplusplus" alt="C++17">
  <img src="https://img.shields.io/badge/platforms-Linux%20%7C%20macOS-lightgrey" alt="Platforms: Linux | macOS">
  <a href="https://crowdin.com/project/nexis"><img src="https://badges.crowdin.net/nexis/localized.svg" alt="Crowdin"></a>
</p>

## Features

- **Dashboard** -- Real-time CPU, memory, disk, and network monitoring with circular gauges
- **GPU Monitoring** -- GPU utilization tracking for NVIDIA, AMD, and Intel (Linux); Apple Silicon (macOS)
- **System Cleaner** -- Remove package caches, crash reports, application logs, and app caches
- **Process Manager** -- View, sort, and manage running processes
- **Service Manager** -- Start, stop, and toggle system services (systemd / launchd)
- **Startup Apps** -- Manage auto-start applications with configurable delay
- **Package Uninstaller** -- Uninstall packages via APT, DNF, Pacman, Snap (Linux) or Homebrew (macOS)
- **Resource Monitor** -- Historical charts for CPU, memory, network, and disk I/O
- **Hosts File Editor** -- Manage `/etc/hosts` entries
- **APT Source Manager / Homebrew Taps** -- Manage package repositories
- **Desktop Settings** -- Adjust GNOME (Linux) or macOS desktop preferences
- **Theme Support** -- Dark, light, and system-auto color schemes
- **Internationalization** -- 26 languages supported

## Why This Fork?

The [original project](https://github.com/oguzhaninan/Stacer) by oguzhaninan is no longer actively maintained. This fork picks up where it left off:

| | Upstream (oguzhaninan) | This Fork |
|---|---|---|
| **Status** | Inactive since 2020 | Actively maintained |
| **Qt version** | Qt 5 | Qt 6 with C++17 |
| **Platforms** | Linux only | Linux and macOS |
| **GPU monitoring** | Not available | NVIDIA, AMD, Intel, Apple Silicon |
| **Themes** | Single theme | Dark, light, and system-auto |
| **Known bugs** | 50+ open issues | Critical bugs fixed (see [BUGS.md](BUGS.md)) |

## Downloads

Pre-built binaries are available on the [Releases page](https://github.com/lsimpsonsfdc/Nexis/releases/latest):

- **Linux x86_64**: `.deb` package, `.AppImage` portable, standalone binary
- **macOS Apple Silicon**: `.dmg` disk image

## Building from Source

### Prerequisites

#### Linux

Install Qt 6 development libraries and the Adwaita icon theme:

**Ubuntu / Debian:**
```bash
sudo apt install qt6-base-dev qt6-charts-dev qt6-svg-dev qt6-tools-dev-tools \
  libqt6concurrent6 adwaita-icon-theme cmake g++
```

**Fedora / RHEL:**
```bash
sudo dnf install qt6-qtbase-devel qt6-qtcharts-devel qt6-qtsvg-devel \
  qt6-linguist adwaita-icon-theme cmake gcc-c++
```

**Arch Linux:**
```bash
sudo pacman -S qt6-base qt6-charts qt6-svg qt6-tools adwaita-icon-theme cmake
```

#### macOS

Install dependencies via Homebrew. The **adwaita-icon-theme** package is required for consistent icon rendering -- without it, many UI icons will appear blank.

```bash
brew install qt@6 cmake adwaita-icon-theme
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

## Screenshots

<p align="center">
    <img src="https://raw.githubusercontent.com/lsimpsonsfdc/Nexis/native/screenshots/Nexis-01.png" width="700">
</p>

<p align="center">
    <img src="https://raw.githubusercontent.com/lsimpsonsfdc/Nexis/native/screenshots/Nexis-02.png" width="700">
</p>

<p align="center">
    <img src="https://raw.githubusercontent.com/lsimpsonsfdc/Nexis/native/screenshots/Nexis-03.png" width="700">
</p>

<p align="center">
    <img src="https://raw.githubusercontent.com/lsimpsonsfdc/Nexis/native/screenshots/Nexis-04.png" width="700">
</p>

<p align="center">
    <img src="https://raw.githubusercontent.com/lsimpsonsfdc/Nexis/native/screenshots/Nexis-05.png" width="700">
</p>

<p align="center">
    <img src="https://raw.githubusercontent.com/lsimpsonsfdc/Nexis/native/screenshots/Nexis-06.png" width="700">
</p>

<p align="center">
    <img src="https://raw.githubusercontent.com/lsimpsonsfdc/Nexis/native/screenshots/Nexis-07.png" width="700">
</p>

<p align="center">
    <img src="https://raw.githubusercontent.com/lsimpsonsfdc/Nexis/native/screenshots/Nexis-08.png" width="700">
</p>

<p align="center">
    <img src="https://raw.githubusercontent.com/lsimpsonsfdc/Nexis/native/screenshots/Nexis-09.png" width="700">
</p>

<p align="center">
    <img src="https://raw.githubusercontent.com/lsimpsonsfdc/Nexis/native/screenshots/Nexis-10.png" width="700">
</p>

<p align="center">
    <img src="https://raw.githubusercontent.com/lsimpsonsfdc/Nexis/native/screenshots/Nexis-11.png" width="700">
</p>

## Development

This fork is co-authored by [Claude Code](https://claude.ai/claude-code), Anthropic's AI coding agent. Claude Code contributes to architecture decisions, feature implementation, bug fixes, CI/CD pipelines, and release engineering -- working alongside the human maintainer as a pair-programming partner.

## Translations

Nexis supports 26 languages. Translations are managed via [Crowdin](https://crowdin.com/project/nexis) -- no coding required to contribute. Sign up, pick a language, and start translating in your browser.

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

Bug reports and feature requests are tracked in [BUGS.md](BUGS.md) and [FEATURE_REQUESTS.md](FEATURE_REQUESTS.md). Feel free to [open an issue](https://github.com/lsimpsonsfdc/Nexis/issues) or submit a pull request.

<!--

This project exists thanks to all the people who have previously contributed. [[Contribute](CONTRIBUTING.md)].</br>
Please let me know if you would like to join in the fun.
<a href="https://github.com/lsimpsonsfdc/Nexis/graphs/contributors"><img src="https://opencollective.com/Nexis/contributors.svg?width=890&button=false" /></a>


### Financial Contributors

Become a financial contributor and help us sustain our community. [[Contribute](https://opencollective.com/Nexis/contribute)]

#### Individuals

<a href="https://opencollective.com/Nexis"><img src="https://opencollective.com/Nexis/individuals.svg?width=890"></a>

#### Organizations

Support this project with your organization. Your logo will show up here with a link to your website. [[Contribute](https://opencollective.com/Nexis/contribute)]

<a href="https://opencollective.com/Nexis/organization/0/website"><img src="https://opencollective.com/Nexis/organization/0/avatar.svg"></a>
<a href="https://opencollective.com/Nexis/organization/1/website"><img src="https://opencollective.com/Nexis/organization/1/avatar.svg"></a>
<a href="https://opencollective.com/Nexis/organization/2/website"><img src="https://opencollective.com/Nexis/organization/2/avatar.svg"></a>
<a href="https://opencollective.com/Nexis/organization/3/website"><img src="https://opencollective.com/Nexis/organization/3/avatar.svg"></a>
<a href="https://opencollective.com/Nexis/organization/4/website"><img src="https://opencollective.com/Nexis/organization/4/avatar.svg"></a>
<a href="https://opencollective.com/Nexis/organization/5/website"><img src="https://opencollective.com/Nexis/organization/5/avatar.svg"></a>
<a href="https://opencollective.com/Nexis/organization/6/website"><img src="https://opencollective.com/Nexis/organization/6/avatar.svg"></a>
<a href="https://opencollective.com/Nexis/organization/7/website"><img src="https://opencollective.com/Nexis/organization/7/avatar.svg"></a>
<a href="https://opencollective.com/Nexis/organization/8/website"><img src="https://opencollective.com/Nexis/organization/8/avatar.svg"></a>
<a href="https://opencollective.com/Nexis/organization/9/website"><img src="https://opencollective.com/Nexis/organization/9/avatar.svg"></a>
-->
