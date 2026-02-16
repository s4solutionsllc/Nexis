# Nexis Feature Matrix

A comparison of features available on each supported platform.

| Feature | Linux | macOS | Notes |
|---|:---:|:---:|---|
| **Dashboard** | Yes | Yes | CPU, Memory, Disk, GPU, and Network monitoring |
| **Dashboard Kiosk Mode** | Yes | Yes | F11 fullscreen dashboard-only mode; ESC to exit |
| **Hardware Info** | Yes | Yes | System, Processor, Graphics, and Memory details |
| **Startup Apps** | Yes | Yes | Linux: `.desktop` files in `~/.config/autostart/`; macOS: LaunchAgents plists |
| **System Cleaner** | Yes | Yes | Package caches, crash reports, app logs, app caches, trash |
| **File Search** | Yes | Yes | Linux: GNU `find`; macOS: BSD `find` |
| **Services Manager** | Yes | Yes | Linux: `systemctl`; macOS: `launchctl` |
| **Process Manager** | Yes | Yes | View and manage running processes |
| **Package Uninstaller** | Yes | Yes | Linux: APT/DNF/YUM/Pacman; macOS: Homebrew |
| **Resource Monitor** | Yes | Yes | Historical CPU, Memory, GPU, Network, and Disk I/O charts |
| **Helpers (Hosts/DNS)** | Yes | Yes | Edit `/etc/hosts` file |
| **Source Manager** | Yes | Yes | Linux: APT sources/PPAs; macOS: Homebrew taps |
| **Desktop Settings** | Yes | Yes | Linux: GNOME `gsettings`; macOS: `defaults` command |
| **Settings** | Yes | Yes | Theme selection, language, autostart toggle |
| **Thermal Monitoring** | Yes | Yes | Linux: `hwmon` sensors; macOS: `osx-cpu-temp` |
| **Snap Packages** | Yes | No | Linux-only package format |
| **Theme Support** | Yes | Yes | Dark and light themes |
| **Internationalization** | Yes | Yes | 34 languages |

## Platform-Specific Details

### Linux
- **Package Managers**: APT (Debian/Ubuntu), DNF (Fedora), YUM (RHEL/CentOS), Pacman (Arch), Snap
- **Service Manager**: systemd via `systemctl`
- **Privilege Escalation**: PolicyKit via `pkexec`
- **System Info**: Reads from `/proc/`, `/sys/`, and standard Linux utilities
- **Packaging**: `.deb` package available

### macOS
- **Package Manager**: Homebrew (formulae and casks)
- **Service Manager**: launchd via `launchctl`
- **Privilege Escalation**: AppleScript via `osascript`
- **System Info**: Uses Mach kernel APIs, `sysctl`, and `~/Library/` paths
- **Packaging**: `.app` bundle
