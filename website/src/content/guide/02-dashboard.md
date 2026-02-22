---
title: "Dashboard"
description: "Understand the real-time monitoring tiles, kiosk mode, and update checker on the Dashboard."
order: 2
icon: "layout-dashboard"
---

# Dashboard

The Dashboard is the first thing you see when Nexis launches. It presents a **bento grid** of live-updating tiles that summarize the health and activity of your system at a glance. Every metric refreshes automatically -- most once per second -- so you always have a current picture without needing to click anything.

![Dashboard overview](/Nexis/images/guide/dashboard-overview.png)

## Hero Row

The top row contains the largest tiles and the most critical information.

### HeroCard (CPU + Memory)

The wide tile spanning the first two columns combines **CPU** and **Memory** into a single card separated by a vertical divider. Each half shows:

- A percentage value in large text
- A sparkline chart tracking recent history
- A label identifying the metric

This dual layout lets you spot correlated spikes -- for example, a build process that drives both CPU and memory up at the same time.

### Disk Tile

The Disk tile features a **donut chart** that fills proportionally to your disk usage. Inside the ring you will see the used and total capacity. Below the chart, a health badge reports the drive's SMART verdict (e.g., "Good (92%)").

> **Tip:** If your machine has more than one disk, a small gear icon appears in the top-right corner of the tile. Click it to open a dropdown menu and switch which drive is displayed. Your selection is remembered across sessions.

![Disk tile with gear selector](/Nexis/images/guide/dashboard-disk-gear.png)

### Network Tile

The Network tile shows two sparkline charts stacked vertically -- one for **Download** speed and one for **Upload** speed. A horizontal divider separates the two. The name of the active network interface is displayed beneath the charts.

## Conditional Tiles (Row 2)

The second row contains tiles that appear **only when the corresponding hardware is present**. If your machine does not have a discrete GPU, temperature sensors, or a battery, those tiles are simply absent -- the layout adjusts automatically.

### GPU Tile

Shows the current GPU utilization as a percentage with a sparkline history. If your system has multiple GPUs, a selector lets you choose which one to monitor.

### Temperature Tile

Displays the reading from a selected thermal sensor. When two or more sensors are detected, a gear icon menu appears so you can pick which sensor to track.

### Battery Tile

Shows the current charge level as a percentage. This tile only appears on laptops and other battery-powered devices.

> **Linux:** GPU data comes from sysfs (AMD/Intel) or `nvidia-smi` (NVIDIA). Temperature data comes from `/sys/class/hwmon/`.

> **macOS:** GPU info is read through IOKit and Metal. Temperature data comes from the System Management Controller (SMC).

## System Summary Bar

A full-width bar below the tiles shows a compact, single-line summary of your system: **hostname** in bold, followed by your OS name, CPU model, and total RAM. This gives you a quick identity check, especially useful if you manage multiple machines.

## Footer Status Bar

At the very bottom of the Dashboard, a small status bar displays the current Nexis version and the data refresh interval.

## Kiosk Mode

Kiosk mode turns Nexis into a fullscreen monitoring display by hiding the sidebar, title bar, and window chrome. This is ideal for a dedicated monitoring screen, a wall-mounted display, or simply when you want a distraction-free view.

There are three ways to enter and exit kiosk mode:

| Method | Enter | Exit |
|--------|-------|------|
| Keyboard | <kbd>F11</kbd> | <kbd>F11</kbd> or <kbd>ESC</kbd> |
| System tray | Right-click tray icon, check "Kiosk Mode (F11)" | Uncheck the same menu item |
| Dashboard button | Click the fullscreen icon in the top-right corner | Click the collapse icon that replaces it |

When you activate kiosk mode, a brief overlay message ("Press ESC to exit kiosk mode") fades in and then disappears after a few seconds. Your kiosk mode preference is saved, so if you quit and relaunch Nexis, it returns to kiosk mode automatically.

![Kiosk mode overlay](/Nexis/images/guide/dashboard-kiosk.png)

> **Tip:** Kiosk mode keeps the data refresh timers running even when the window would normally be considered "minimized." This ensures the Dashboard stays up to date on a dedicated display.

## Update Checker

Nexis periodically checks GitHub for new releases. If a newer version is available, a notification appears on the Dashboard so you know when it is time to upgrade.

## What's Next

To dive deeper into the hardware details behind the Dashboard numbers, see the [Hardware Info](./03-hardware-info) guide.
