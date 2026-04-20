# Bundle A — Cold Launch Sprint: UAT Checklist (FR-96, FR-97, FR-98)

Plain-language acceptance tests for the Cold Launch Sprint. The three FRs are cross-coupled — verify them together. Each check is designed for a non-technical reader.

**Build under test:** Commits `34652f0` (A) → `4d8ce43` (B) → `1626073` (C) → `11d8947` (D) → `28bc13e` (E), on `native`.

---

## 1. App launch feels snappier

- [ ] Quit Nexis completely (Cmd-Q on macOS).
- [ ] Double-click the Nexis icon.
- [ ] **Expect:** The splash screen disappears noticeably faster than you remember. The Dashboard tile grid should be visible and already showing CPU/memory numbers within a second of clicking the icon.
- [ ] Disk health information on the Disk tile (if it shows one) may briefly be empty, then fills in a moment later — that is correct, discovery now happens in the background.

## 2. Pages still work

Click each sidebar item in order and confirm the page appears normally. The first click on any given page (per session) may take a fraction of a second longer than subsequent clicks — that is expected (the page is being built on demand).

- [ ] **Dashboard** — tiles populate, gauges animate, sparklines advance.
- [ ] **Hardware Info** — CPU model, cores, memory size, battery details, fan sensors, drives all populate. Drive list should show all physical drives with SMART data.
- [ ] **Resources** — five or more charts render (CPU, CPU Load Avg, Disk R/W, Memory, Network, optional GPU, optional Disk Temperature).
- [ ] **System Cleaner** — category list populates, each row has an icon.
- [ ] **Disk Tools** — "Large & Old Files" and "Duplicates" sub-pages both render.
- [ ] **Search** — search box and results table appear.
- [ ] **Processes** — process list populates within a couple of seconds.
- [ ] **Services** — service list populates.
- [ ] **Startup Apps** — startup items list renders. Toggle switches work.
- [ ] **Applications / Uninstaller** — installed apps populate (macOS: apps; Linux: packages + snaps).
- [ ] **Helpers** — sub-sections (Host Manage, Network Diagnostics, Open Ports, Firewall) visible.
- [ ] **System Logs** — log entries populate.
- [ ] **Settings** — all sections render: Theme, Font, Scheduled Clean, Alerts, Startup, etc. The "Disk health alert" checkbox should be visible (as long as your system has any drives).

## 3. Navigation via keyboard / command palette

- [ ] Press **Ctrl+K** to open the command palette.
- [ ] Type each page name ("Hardware Info", "Resources", "Processes", etc.) and press Enter.
- [ ] **Expect:** Each page opens correctly. Pages visited via command palette for the first time should still construct and show their content.

## 4. Navigation via tray icon

- [ ] Close the Nexis window (don't quit — on macOS click the red dot, on Linux click X with "Minimize to tray" enabled).
- [ ] Right-click the Nexis tray icon.
- [ ] Click each page name in the tray menu.
- [ ] **Expect:** The window reappears on the selected page. Page content populates correctly.

## 5. Dashboard disk-health information appears

- [ ] Launch Nexis cold.
- [ ] Stay on the Dashboard (don't navigate away).
- [ ] **Expect:** Within 1-3 seconds after launch, the Disk tile shows a health indicator (SMART status, a color badge, or similar). On systems with multiple drives, all drives should be accounted for.
- [ ] **Expect:** The Health Score tile (top-right area) should include SMART data in its weighting once drives appear — the score may shift subtly when disk data arrives.

## 6. Resources page disk-temperature chart

*Only applies if your system has drives that report temperature via SMART — most modern SSDs do.*

- [ ] Launch Nexis cold, navigate directly to **Resources**.
- [ ] **Expect:** Within a few seconds, a "History of Disk Temperature" chart appears at the bottom of the charts list (above the "Disk Usage Analyzer" launcher).
- [ ] Wait 30+ seconds and verify the chart's lines advance — temperature data refreshes every 30 s.

## 7. Settings "Disk health alert" checkbox

- [ ] Launch Nexis cold. Go to **Settings**.
- [ ] Scroll to the Alerts section.
- [ ] **Expect:** "Disk health alert" checkbox is visible (as long as your system has drives with SMART data). Toggle it and confirm the state persists across restart.

## 8. Hardware Info lazily populates

- [ ] Launch Nexis cold.
- [ ] Stay on Dashboard for 5 seconds without visiting Hardware Info.
- [ ] Now click **Hardware Info**.
- [ ] **Expect:** The page appears; populate may take a brief moment on the first visit; subsequent visits are instant.
- [ ] Click Hardware Info again (navigate away and back): should be instant — no re-populate.

## 9. Kiosk mode still works

- [ ] Press **F11** (or use the Dashboard kiosk button if visible).
- [ ] **Expect:** Sidebar hides, Dashboard goes full-screen.
- [ ] Press **Escape** to exit kiosk.
- [ ] **Expect:** Sidebar reappears, Dashboard normal.

## 10. Theme switching

- [ ] In **Settings**, change the theme from Light to Dark (or vice versa).
- [ ] **Expect:** All pages (including ones you haven't visited yet) switch theme immediately when you navigate to them.
- [ ] Visit a page you haven't opened since launch after the theme change — confirm its colors match the selected theme.

## 11. No stale or empty drive state

*Regression check for the one-shot `hasDiskHealth` readers that Commit E patched.*

- [ ] Launch Nexis cold.
- [ ] Immediately navigate to **Resources** (within ~1 second of launch, before disk discovery completes).
- [ ] Wait 5-10 seconds.
- [ ] **Expect:** The disk-temperature chart appears once discovery completes — it should NOT require a theme toggle or app restart to appear.

## 12. Idle RAM footprint improved

*Optional — requires Activity Monitor or `ps`.*

- [ ] Launch Nexis cold, stay on Dashboard for 10 seconds without visiting any other page.
- [ ] Check memory usage (Activity Monitor on macOS, `ps aux | grep nexis` on Linux).
- [ ] **Expect:** Idle RAM is noticeably lower than before Bundle A (target: ~50-120 MB less, depending on system). Exact numbers vary by platform.

---

## Sign-off

When all 12 sections are checked and no unexpected behavior is observed, Bundle A is considered UAT-passed and FR-96, FR-97, FR-98 can be marked `[x]` in `FEATURE_REQUESTS.md`.

If any section fails, note the FR number (96/97/98) in the failure and report back.
