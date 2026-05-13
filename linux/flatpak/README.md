# Nexis Flatpak Packaging

App ID: `io.github.s4solutionsllc.Nexis`  
Manifest: `io.github.s4solutionsllc.Nexis.yml`  
Runtime: `org.kde.Platform//6.7` (KDE Qt6 SDK)

---

## Prerequisites

```bash
# Install flatpak-builder
sudo apt install flatpak-builder     # Debian/Ubuntu
sudo dnf install flatpak-builder     # Fedora

# Add Flathub remote
flatpak remote-add --if-not-exists flathub https://dl.flathub.org/repo/flathub.flatpakrepo

# Install KDE runtime and SDK (~2-3 GB)
flatpak install flathub org.kde.Platform//6.7 org.kde.Sdk//6.7
```

---

## Build

```bash
# From repo root
flatpak-builder --force-clean --sandbox /tmp/nexis-build \
  linux/flatpak/io.github.s4solutionsllc.Nexis.yml

# Install locally for testing
flatpak-builder --force-clean --install --user /tmp/nexis-build \
  linux/flatpak/io.github.s4solutionsllc.Nexis.yml
```

---

## Privileged Operation Test Checklist

Run these after `flatpak run io.github.s4solutionsllc.Nexis`. Each test
verifies that the sandbox permissions are sufficient for the feature.

| # | Feature | Test | Expected |
|---|---|---|---|
| 1 | **Services Manager** | Navigate to Services; start/stop any user service; toggle enable/disable | Status updates; no "permission denied" errors in logs |
| 2 | **System Cleaner** | Run scan; confirm deletion of a cache item in `~/.cache` | Files deleted; log shows correct count |
| 3 | **APT Repository Manager** | Open page; list loads; add a test PPA (then remove it) | PPA appears in list; file written to `/etc/apt/sources.list.d/` |
| 4 | **Disk Tools — S.M.A.R.T.** | Open Disk Tools; select a drive; click Health | SMART data shown (or "SMART not supported" if VM/NVMe) |
| 5 | **CPU Turbo/Frequency** | Open Helpers → CPU Tuning; read current freq; change governor | Governor change applied; `/sys/...cpufreq/scaling_governor` updated |
| 6 | **Swappiness** | Open Helpers → Swappiness; change value; click Apply | Value written to `/proc/sys/vm/swappiness`; confirmed by re-read |
| 7 | **SSD TRIM** | Open Helpers → SSD TRIM; click "Run TRIM Now" | fstrim output shown; no errors |
| 8 | **Firewall** | Open Helpers → Firewall; list rules | Rules displayed (or "ufw not installed" on non-ufw systems) |
| 9 | **Process — Kill** | Open Processes; right-click a process; End Process | Process terminates (own processes); polkit dialog for root processes |
| 10 | **Uninstaller** | Open Uninstaller; select an installed package; uninstall | dpkg/apt uninstall dialog appears; polkit escalation works |

### Log monitoring during tests

```bash
# Watch for sandbox policy violations in real time
journalctl -f | grep -E "(flatpak|audit|DENIED)"

# Run app with verbose logging
flatpak run --env=QT_LOGGING_RULES="*=true" io.github.s4solutionsllc.Nexis 2>&1 | tee /tmp/nexis-flatpak.log
```

---

## Flathub Submission

1. Fork https://github.com/flathub/flathub
2. Create a new directory: `io.github.s4solutionsllc.Nexis/`
3. Copy `io.github.s4solutionsllc.Nexis.yml` into it
4. Open a PR to `flathub/flathub`
5. In the PR description, link to `docs/flatpak-reviewer-justification.md`
   for the `--filesystem=host` and `--device=all` justification

---

## Notes

- `rename-icon` and `rename-desktop-file` in the manifest handle Flathub's
  app-id naming convention automatically — no source asset changes needed.
- `x-checker-data` is configured for Flathub's external-data-checker bot;
  it will auto-update `tag` and `commit` on each new GitHub release.
- See `docs/flatpak-reviewer-justification.md` for the full write-up on
  `--filesystem=host` and `--device=all` permissions.
