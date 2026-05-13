# Flathub Reviewer Justification — Nexis

**App ID:** `io.github.s4solutionsllc.Nexis`  
**Manifest:** `linux/flatpak/io.github.s4solutionsllc.Nexis.yml`  
**Related issue:** [SSO-93](/SSO/issues/SSO-93)

---

## Summary

Nexis is a Linux system optimizer and monitoring tool. It must observe and
interact with nearly every subsystem on the host: processes, filesystems,
hardware sensors, network interfaces, system services, and privileged
operations such as package management and CPU tuning.

This document justifies the two permissions that are likely to draw reviewer
scrutiny: `--filesystem=host` and `--device=all`.

---

## 1. `--filesystem=host`

### Why it is required

Nexis performs the following operations that collectively require read and/or
write access across the host filesystem:

| Feature | Host paths accessed |
|---|---|
| **Dashboard** — CPU, memory, swap, disk, GPU, network tiles | `/proc/stat`, `/proc/meminfo`, `/proc/net/*`, `/sys/class/net/*`, `/sys/class/hwmon/*`, `/sys/class/drm/*`, `/sys/block/*` |
| **Hardware Info** | `/sys/class/*`, `/proc/cpuinfo`, `/proc/version`, DMI under `/sys/class/dmi/id/` |
| **Process Manager** | `/proc/<pid>/stat`, `/proc/<pid>/status`, `/proc/<pid>/cmdline`, `/proc/<pid>/io`, `/proc/<pid>/smaps_rollup` |
| **Resources charts** — including CPU PSI | `/proc/pressure/*`, `/sys/class/hwmon/*` |
| **System Cleaner** | User's home directory and system cache dirs (`/tmp`, `/var/cache`, `~/.cache`, etc.) |
| **APT Repository Manager** | `/etc/apt/sources.list`, `/etc/apt/sources.list.d/` (read + write) |
| **Services Manager** | Invokes `systemctl` (host binary at `/usr/bin/systemctl`) |
| **Startup Apps Manager** | `~/.config/autostart/`, `~/.config/systemd/user/` |
| **Disk Tools** | Block device nodes under `/dev/`, `/sys/block/` |
| **Uninstaller** | Installed file manifests under `/var/lib/dpkg/`, `/var/lib/apt/` |
| **System Logs viewer** | `/var/log/` tree |
| **Network Diagnostics** | Executes `ping`, `traceroute` from host `/usr/bin/` |
| **Helpers** — swappiness, CPU freq, SSD TRIM, Wake-on-LAN | `/proc/sys/vm/swappiness`, `/sys/devices/system/cpu/*/cpufreq/`, invokes `fstrim`, `ethtool` |

A portal-per-path approach is not viable because:

1. Many paths are pseudo-filesystems (`/proc`, `/sys`) that the XDG portals do
   not cover.
2. The user-facing paths (System Cleaner, APT sources) are not known at
   install time; they are determined dynamically at runtime.
3. Host binary invocations via `QProcess` require that `/usr/bin`, `/usr/sbin`,
   etc. are on the sandbox PATH — covered by `--filesystem=host` combined with
   the PATH env override in the manifest.

### Accepted precedent

`--filesystem=host` is accepted by Flathub for system-level monitoring apps.
Current examples in the Flathub catalogue that use this permission:

- **GNOME Usage** (`org.gnome.Usage`) — process and resource monitor
- **Stacer** (`com.oguzhaninan.Stacer`) — the upstream this project is forked
  from
- **htop** (`io.github.htop_dev.htop`) — process viewer
- **Cockpit Client** — system management tool

Nexis's use case is identical: a trusted local desktop application that the
user explicitly installs to observe and manage their own system.

### Access is read-heavy, write is scoped

The vast majority of Nexis's filesystem access is **read-only** (metrics,
logs, hardware info). Write access is limited to:

- APT sources (user-initiated, with polkit escalation)
- Cleaner deletions (user-selected, with explicit confirmation)
- Startup app entries in the user's home directory

---

## 2. `--device=all`

GPU utilisation, VRAM usage, and per-process GPU metrics are read from:

- `/dev/dri/card*` and `/dev/dri/renderD*` — DRI device nodes (AMD/Intel)
- NVIDIA: `nvidia-smi` reads via `/dev/nvidia*`
- Temperature and fan sensors: `/dev/hwmon*` (some distributions only expose
  these via device nodes, not solely via `/sys`)

Disk S.M.A.R.T. queries (ioctl-based) require access to block device nodes
(`/dev/sda`, `/dev/nvme*`, etc.).

`--device=dri` alone is insufficient because it does not cover NVIDIA device
nodes or block devices needed for S.M.A.R.T.

---

## 3. System D-Bus names

| Name | Reason |
|---|---|
| `org.freedesktop.PolicyKit1` | Privilege escalation for APT, CPU tuning, firewall, disk operations. |
| `org.freedesktop.systemd1` | Services Manager: list, start, stop, enable, disable systemd units. |
| `org.freedesktop.UDisks2` | Disk Tools: S.M.A.R.T. health queries, partition information. |
| `org.freedesktop.hostname1` | System Info page: machine hostname and machine-id. |
| `org.freedesktop.login1` | Session information on the Hardware Info page. |
| `org.freedesktop.NetworkManager` | Network interface listing and statistics. |

---

## 4. Why not Option B (individual D-Bus portals)?

Option B would replace QProcess-based host tool invocations with direct D-Bus
API calls to each service. While architecturally cleaner for a purpose-built
D-Bus client, it is not feasible for Nexis without significant refactoring:

- Many operations (APT source editing, file tree scanning, log reading) have
  no corresponding D-Bus portal and would still require filesystem access.
- The systemd D-Bus API surface for the full feature set (unit files, overrides,
  transient services) is substantially more complex than `systemctl` CLI.
- The net permission delta vs. Option A is minimal — the same D-Bus names plus
  filesystem access to pseudo-filesystems would still be required.

Option A (`--filesystem=host` + polkit) is the approach used by the upstream
Stacer project and by every comparable system monitor on Flathub.
