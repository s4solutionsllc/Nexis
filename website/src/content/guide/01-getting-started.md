---
title: "Getting Started"
description: "Install Nexis and take your first look around the interface."
order: 1
icon: "rocket"
---

# Getting Started

Nexis is a system optimizer and monitoring tool for Linux and macOS. It gives you real-time insight into your hardware, helps you clean up junk files, manage startup apps and services, and much more -- all from a single, polished interface.

This guide walks you through installation and introduces the main parts of the app so you can find your way around quickly.

## Installation

### Linux

**APT (Ubuntu / Debian, recommended)**

Install from the PPA to get automatic updates through `apt`:

```bash
sudo add-apt-repository ppa:s4solutionsllc/nexis
sudo apt update
sudo apt install nexis
```

Supports Ubuntu 26.04 (Resolute) on x86_64 and ARM64. Ubuntu 22.04, 24.04, and Linux Mint Zena aren't supported via the PPA (Nexis requires Qt 6.8 LTS, which isn't available in those repos) -- use the AppImage instead.

**DNF / COPR (Fedora)**

```bash
sudo dnf copr enable luke-s4solutions/nexis
sudo dnf install nexis
```

Supports Fedora 43, Fedora 44, and Rawhide on x86_64. COPR is Fedora's community build service, not an official Fedora repository, so `dnf` will ask you to confirm before adding it.

**Debian / Ubuntu (.deb, manual)**

Download the latest `.deb` package from the [Releases page](https://github.com/s4solutionsllc/Nexis/releases) and install it:

```bash
sudo dpkg -i nexis_*.deb
sudo apt-get install -f   # resolve any missing dependencies
```

**AppImage (any distro)**

Download the `.AppImage` file, make it executable, and run:

```bash
chmod +x Nexis-*.AppImage
./Nexis-*.AppImage
```

> **Tip:** Move the AppImage to `~/Applications/` or `/opt/` so it has a permanent home. Most desktop environments let you right-click and "Allow executing as program" instead of using the terminal.

### macOS

**Homebrew (recommended)**

```bash
brew tap s4solutionsllc/nexis
brew trust s4solutionsllc/nexis
brew install --cask nexis
```

The tap updates automatically on each release. Homebrew 6.0+ requires the one-time `brew trust` step above before it will load the cask.

**Manual (.dmg)**

Download the `.dmg` file from the [Releases page](https://github.com/s4solutionsllc/Nexis/releases). Open it and drag **Nexis** into your **Applications** folder. On first launch, macOS may ask you to confirm you want to open an app from an identified developer.

> **macOS:** Nexis is built natively for Apple Silicon. macOS 14 (Sonoma) or later is required. Intel Macs are not currently supported.

### Building from Source

If you prefer to compile Nexis yourself, make sure you have **Qt 6**, **CMake 3.16+**, and a C++17-capable compiler installed.

```bash
git clone https://github.com/s4solutionsllc/Nexis.git
cd Nexis
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

> **macOS:** Point CMake at your Qt installation with `-DCMAKE_PREFIX_PATH=$(brew --prefix qt@6)` and replace `nproc` with `sysctl -n hw.ncpu`.

The compiled binary will be in the `build/` directory. Run it directly or install it with `cmake --install build`.

## First Launch

When Nexis opens for the first time, you land on the **Dashboard** -- a live overview of your CPU, memory, disk, network, and more. Everything updates in real time so you can see your system's health at a glance.

![Nexis Dashboard on first launch](/Nexis/images/guide/first-launch.png)

## The Sidebar

The left-hand sidebar is your main navigation. It is organized into five groups that match the type of task you want to do -- **MONITOR**, **DIAGNOSE**, **CLEAN**, **MANAGE**, and **TOOLS** -- with **Settings** pinned at the very bottom.

### MONITOR

Always expanded, with no section header to click:

- **Dashboard** -- Real-time gauges and sparklines for CPU, memory, disk, network, GPU, temperature, and battery.
- **Resources** -- Historical charts showing the last 60 seconds of system activity.
- **Processes** -- View and end running processes.
- **Network Usage** -- Per-interface data usage tracking with a monthly cap.

### DIAGNOSE

- **Hardware Info** -- A detailed inventory of your hardware components.
- **Boot Analysis** -- A ranked breakdown of what's slowing down your startup.
- **System Logs** -- Browse and filter system log entries with severity color-coding.

### CLEAN

- **System Cleaner** -- Scan for and remove junk files.
- **Disk Tools** -- Find large/old files and detect duplicates to reclaim disk space.
- **Disk Map** -- Visualize disk usage with an interactive treemap, bubble map, or sunburst.
- **Mail Cleanup** (macOS) -- Clean up Mail.app storage.
- **File Shredder** -- Securely overwrite and delete files or folders you select.
- **Search** -- Find files across your filesystem by name, size, date, and more.

### MANAGE

- **Uninstaller** (labeled **Applications** on macOS) -- Remove installed applications and packages.
- **Startup Apps** -- Control which apps launch at login.
- **Services** -- Start, stop, and configure system daemons.
- **APT Repositories** (Linux) / **Homebrew** (macOS) -- Manage package sources.
- **Docker** -- Manage Docker images, containers, and volumes (shown only when Docker is installed).

### TOOLS

- **Helpers** -- Utility tools such as the Hosts File Manager and DNS cache flushing.
- **GNOME Settings** (Linux/GNOME) -- Tweak desktop appearance, mouse, and window behavior.

### Pinned Footer

- **Settings** -- Configure Nexis itself (theme, language, alerts, scheduled cleaning, and more).
- **Feedback** -- Opens a feedback dialog rather than navigating to a page.

> **Tip:** Pages that do not apply to your platform are hidden automatically. You will never see a grayed-out button for something your OS does not support.

## Collapsible Sidebar

Press <kbd>Ctrl</kbd>+<kbd>B</kbd> (or click the collapse button at the top of the sidebar) to shrink the sidebar into a narrow icon rail. This gives more room to the main content area while still letting you switch pages with a single click. Press <kbd>Ctrl</kbd>+<kbd>B</kbd> again to expand it.

![Sidebar in collapsed and expanded states](/Nexis/images/guide/sidebar-collapsed.png)

### Collapsible Section Groups

The **DIAGNOSE**, **CLEAN**, **MANAGE**, and **TOOLS** section headers are clickable (**MONITOR** has no header and is always expanded). Click a section header to **collapse** its group, hiding the page buttons underneath. Click again to expand. A small chevron icon indicates whether the section is expanded or collapsed.

This is useful when you only work with a subset of pages — collapse the sections you don't use to reduce visual clutter. Nexis remembers which sections are collapsed across sessions.

## Command Palette

Press <kbd>Ctrl</kbd>+<kbd>K</kbd> to open the **Command Palette**, a fuzzy-search popup that lets you jump to any page or run common actions (like toggling kiosk mode or starting a clean) without touching the sidebar. Just start typing and select the result you want.

![Command Palette search popup](/Nexis/images/guide/command-palette.png)

## System Tray Icon

Nexis places an icon in your system tray (notification area). Right-click it to access quick navigation to any page, toggle kiosk mode, or quit the app. If you close the main window, Nexis keeps running in the tray so it can continue sending you threshold alerts and running scheduled cleans.

## What's Next

Now that you know your way around, head to the [Dashboard](./02-dashboard) guide to learn about every tile and feature on the main monitoring screen.
