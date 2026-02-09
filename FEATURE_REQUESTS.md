# Feature Requests

> Managed by Claude Code. Updated across sessions.
> Status: `[ ]` = planned, `[~]` = in progress, `[x]` = done

## From Other Forks (QuentiumYT, etc.)

- [ ] **FR-01: deb822 APT source file support** — Modern `.sources` format used by Debian 13+ / Ubuntu 24.04+. QuentiumYT commit `87279f6`.
- [ ] **FR-02: Single-instance enforcement** — Prevent multiple app copies via QLockFile/QSharedMemory. Focus existing window on re-launch. QuentiumYT issue #12.
- [ ] **FR-03: Expanded cache cleaning (Electron apps, npm, gradle, etc.)** — Scan `~/.config/*/Cache` and `~/.config/*/GPUCache` for Electron apps (Discord, Slack, VSCode). Also npm, bun, gradle, expo caches. QuentiumYT v1.5.0.
- [ ] **FR-04: Background thread cleanup on exit** — Wait for QtConcurrent threads to finish before quitting. QuentiumYT commit `fa70178`.
- [ ] **FR-05: LC_ALL=C for system command parsing** — Force English output so `lscpu` and similar commands parse correctly on non-English systems. QuentiumYT commit `e0b957f`.
- [ ] **FR-06: ARM64 Linux architecture support** — Build for armhf, arm64, i386, powerpc, ppc64el, riscv64, s390x.
- [ ] **FR-07: SVG logo and colorful tray icon** — Redesigned logo in SVG with tray icon that works on light and dark themes. QuentiumYT commit `347bcbe`.
- [ ] **FR-08: Crowdin translation integration** — Professional translation management with automated PR workflows.
- [ ] **FR-09: APT-RPM support (ALT Linux, PCLinuxOS, Vine Linux)** — QuentiumYT PR #31.
- [ ] **FR-10: Startup app customization enhancements** — Expanded options for managing auto-start applications. QuentiumYT v1.5.0.

## From Community Issues (oguzhaninan/Stacer)

- [ ] **FR-11: GPU load / temperature monitoring** — Issues [#105](https://github.com/oguzhaninan/Stacer/issues/105), [#405](https://github.com/oguzhaninan/Stacer/issues/405).
- [ ] **FR-12: Hardware info tab** — Issue [#527](https://github.com/oguzhaninan/Stacer/issues/527).
- [ ] **FR-13: CLI interface** — Issue [#411](https://github.com/oguzhaninan/Stacer/issues/411).
- [ ] **FR-14: Flatpak distribution** — Issue [#493](https://github.com/oguzhaninan/Stacer/issues/493).
- [ ] **FR-15: Autostart delay option** — Issue [#424](https://github.com/oguzhaninan/Stacer/issues/424).
- [ ] **FR-16: Scheduled / automated cleaning** — Issue [#449](https://github.com/oguzhaninan/Stacer/issues/449).
- [ ] **FR-17: Pip cache cleaning** — Issue [#481](https://github.com/oguzhaninan/Stacer/issues/481).
- [ ] **FR-18: System Cleaner exclusion rules** — Issue [#484](https://github.com/oguzhaninan/Stacer/issues/484).
- [ ] **FR-19: Purge vs remove option in uninstaller** — Issue [#484](https://github.com/oguzhaninan/Stacer/issues/484).
- [ ] **FR-20: Docker image / volume management** — Issue [#454](https://github.com/oguzhaninan/Stacer/issues/454).

## Notes

<!-- Claude Code: append new feature requests here. Use the next available FR-XX id. -->
