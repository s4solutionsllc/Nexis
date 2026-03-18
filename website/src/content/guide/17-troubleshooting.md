---
title: "Troubleshooting"
description: "Solutions to common issues and answers to frequently asked questions."
order: 17
icon: "life-buoy"
---

# Troubleshooting

This page covers the most common issues people run into with Nexis and how to resolve them. If your problem is not listed here, you can report it on GitHub using the link at the bottom of this page.

## Dashboard Shows 0% for GPU

If the GPU tile on your Dashboard stays at 0% even though you have a discrete GPU, the issue is usually with the monitoring backend.

**NVIDIA GPUs:** Nexis reads GPU utilization from `nvidia-smi`, the NVIDIA System Management Interface. If it is not installed, Nexis cannot read your GPU's activity. Install it with:

```bash
# Ubuntu/Debian
sudo apt install nvidia-utils-535   # replace 535 with your driver version

# Fedora
sudo dnf install nvidia-smi
```

After installing, verify it works by running `nvidia-smi` in a terminal. If it prints a table with your GPU's name and utilization, Nexis will pick it up on the next refresh.

**AMD and Intel GPUs (Linux):** Nexis reads utilization from sysfs. If the GPU tile shows 0%, your kernel or driver may not expose the required files. Make sure you have up-to-date mesa drivers installed.

> **macOS:** GPU utilization is read through IOKit and Metal. If the tile shows 0%, the GPU may not expose utilization counters through these interfaces. Integrated GPUs on some older Mac models have limited reporting.

## Temperature Shows "No Sensors Found"

The Temperature tile on the Dashboard and the thermal section in Hardware Info rely on discovering temperature sensors through the operating system.

> **Linux:** Sensors are read from `/sys/class/hwmon/`. If this directory is empty or does not contain temperature-capable entries, Nexis has nothing to display. Try running `sensors` from the `lm-sensors` package to verify your system exposes thermal data:

```bash
sudo apt install lm-sensors   # Debian/Ubuntu
sudo sensors-detect            # scan for available sensors
sensors                        # display current readings
```

> **macOS:** Temperature data comes from the System Management Controller (SMC). Most Macs expose at least a CPU temperature sensor. If you see "No sensors found," your Mac model may not expose SMC data to userspace applications.

## Disk Health Shows "Unknown"

The disk health badge on the Dashboard and the Storage section of Hardware Info use SMART data to assess drive health. If a drive shows "Unknown," it means Nexis could not read its SMART attributes.

**Is smartctl installed?** Nexis uses `smartctl` (from the smartmontools package) to query SMART data. Install it if missing:

```bash
# Debian/Ubuntu
sudo apt install smartmontools

# macOS (via Homebrew)
brew install smartmontools
```

**Does it need sudo?** Some drives require root access for SMART queries. If `smartctl` cannot access a drive as a normal user, Nexis will show "Limited data" next to that drive in the Hardware Info Storage section and display an **Unlock** button. Clicking it triggers a polkit authentication prompt (`pkexec`). If you approve, Nexis re-reads the drive with full permissions and updates the health data automatically.

> **Permanent fix:** To avoid the prompt on every launch, grant `smartctl` raw device access once:
> ```bash
> sudo setcap cap_sys_rawio+ep $(which smartctl)
> ```
> After this, Nexis reads SMART data silently without requiring elevation.

**Does the drive support SMART?** USB-attached external drives and some NVMe enclosures do not pass through SMART commands. In these cases, health data is genuinely unavailable.

> **macOS:** Nexis also uses `diskutil` as a supplementary source for disk information. Both tools work together to provide the most complete picture possible.

## Services Page is Empty on macOS

The Services page uses `launchctl` on macOS to list and manage system daemons. Currently, macOS launchd support in Nexis is limited compared to the Linux `systemctl` integration. You may see fewer services than expected, or the page may appear mostly empty.

This is a known limitation. The `launchctl` interface provides less structured output than `systemctl`, making it harder to reliably parse service states. Improvements to macOS service management are planned for future releases.

> **Tip:** In the meantime, you can use the built-in macOS `launchctl list` command in Terminal to view your services, or use the Activity Monitor app for process-level control.

## System Cleaner Cannot Delete Some Files

If the System Cleaner fails to remove certain files, those files are likely owned by root or another system user.

Nexis requests elevated privileges (sudo) when needed, but some environments restrict which files can be deleted even with administrator access. Common scenarios:

- **SIP-protected paths on macOS** -- System Integrity Protection prevents modifications to certain system directories regardless of permissions.
- **Immutable files on Linux** -- Files with the `chattr +i` flag cannot be deleted even by root.
- **Mounted filesystem restrictions** -- Files on read-only mounted partitions cannot be removed.

> **Tip:** If a specific file keeps showing up after cleaning attempts, it is safe to leave it. The System Cleaner will not break anything by failing to delete a protected file.

## The App Will Not Start (Single Instance Lock)

Nexis enforces single-instance behavior to prevent multiple copies from running simultaneously and conflicting with each other. It does this with a lock file at:

```
/tmp/nexis.lock
```

If Nexis crashes or is force-killed, this lock file may not be cleaned up properly. The next time you try to launch Nexis, it sees the stale lock file and thinks another instance is already running, showing a warning dialog instead of starting.

**To fix this**, simply delete the lock file:

```bash
rm /tmp/nexis.lock
```

Then launch Nexis normally. The app will create a fresh lock file on startup.

## How to Report a Bug

If you encounter an issue not covered here, please report it on GitHub:

1. Go to the [Nexis Issues page](https://github.com/nicholasng-7642/Nexis/issues) on GitHub.
2. Click **New Issue** and choose the **Bug Report** template.
3. Include as much detail as possible:
   - Your operating system and version
   - The version of Nexis (shown at the bottom of the Settings page)
   - Steps to reproduce the problem
   - Any error messages you see
   - Screenshots if applicable

You can also reach the Issues page directly from within Nexis by clicking the **Feedback** button at the bottom of the Settings page.

## Building from Source

If you need to build Nexis from source (for example, to test a fix or run on an unsupported distribution), see the installation instructions on the [Getting Started](./01-getting-started) page for a quick guide. For full build system details, visit the [Nexis GitHub repository](https://github.com/nicholasng-7642/Nexis).

## What's Next

That concludes the Nexis User Guide. If you have not explored every page yet, use the sidebar or the [Getting Started](./01-getting-started) page to jump back to any section. Happy monitoring!
