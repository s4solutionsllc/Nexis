# FR-124 & FR-126 — User Acceptance Testing

Target: local dev build (`./build/output/nexis.app` on macOS, `./build/nexis` on Linux)

---

## T1 — FR-124: CPU Pressure Stall Chart (Linux only)

### T1a — Chart visibility gating

- [ ] **Linux with kernel 4.20+ and PSI enabled:** Open Nexis → Resources page. A **"CPU Pressure Stall (some)"** chart is visible below the Disk Temperature chart (or Network chart if no SMART drives).
- [ ] **Linux without PSI support** (kernel < 4.20 or `CONFIG_PSI=n`, confirmed by `cat /proc/pressure/cpu` returning "No such file"): No PSI chart appears. App does not crash.
- [ ] **macOS:** No PSI chart appears. App does not crash.

### T1b — Chart updates live

- [ ] Navigate to the Resources page on Linux. The PSI chart begins updating within 1–2 seconds.
- [ ] The chart shows **three series**: `avg10`, `avg60`, and `avg300`.
- [ ] The legend labels update each second and show a percentage value (e.g., `CPU avg10: 0.3%`).
- [ ] The chart scrolls as a 60-second sliding window — older data moves right, new data appears on the left.

### T1c — Zero cost when hidden

- [ ] Navigate away from the Resources page (e.g., to Dashboard). Navigate back. The chart resumes updating without a spike or gap in the data.
- [ ] No noticeable CPU increase when the Resources page is not active (subscription is unregistered on deactivation).

### T1d — Stress scenario (optional but recommended)

- [ ] Run a CPU-intensive task (e.g., `stress --cpu 4`) while the Resources page is open. Observe that `avg10` rises noticeably while `avg60` and `avg300` lag behind — this is expected behavior for a rolling average.

---

## T2 — FR-126: HTML System Report Export

### T2a — Button presence

- [ ] Open Nexis → Hardware Info page. Two export buttons are visible side by side: **"Export System Report..."** (existing) and **"Export as HTML..."** (new).
- [ ] Both buttons are present on **macOS and Linux**.

### T2b — File dialog

- [ ] Click **"Export as HTML..."**. A save dialog opens with a suggested filename: `nexis-report-YYYY-MM-DD.html`.
- [ ] The dialog filters for HTML files by default.
- [ ] Cancelling the dialog returns to the page without error.

### T2c — Button feedback during generation

- [ ] Click **"Export as HTML..."** and accept the save location. The button briefly shows **"Generating…"** and becomes disabled while the report is built (visible if the update check takes a moment).
- [ ] After generation completes, the button returns to **"Export as HTML..."** and is re-enabled.

### T2d — Success message

- [ ] After a successful export, an information dialog appears: "HTML report saved to {path}".
- [ ] The file exists at the chosen path.

### T2e — File opens in browser

- [ ] Open the generated `.html` file in any browser (Safari, Chrome, Firefox). The page renders without errors.
- [ ] The page title reads **"Nexis System Report"**.
- [ ] A generated timestamp and app version are shown in the header.

### T2f — Report sections

- [ ] **System Overview** section is present — shows at least CPU %, memory used, and memory total as styled metric cards.
- [ ] If a GPU is present: a GPU utilization metric card is shown.
- [ ] If a battery is present: a battery charge % metric card is shown.
- [ ] **Hardware tables** are present: System, Processor, Graphics, Memory. Battery, Fans, and Storage sections appear only if data is available (matching what the Hardware Info page shows).
- [ ] **Top Processes** table is present — shows up to 10 rows with Name, User, CPU %, and Memory columns.
- [ ] **Pending Updates** section is present — shows either a count and list of packages, "No pending updates.", or "Update check unavailable."

### T2g — Self-contained file

- [ ] Move the `.html` file to a different folder (or open it offline). The page still renders correctly with all styling — no broken CSS or missing resources.

### T2h — Existing plain-text export unaffected

- [ ] Click **"Export System Report..."** (the original button). A `.txt` file is generated as before, with no regressions.

### T2i — HTML safety

- [ ] If any hardware field contains special characters (e.g., a hostname with `<` or `&`), the HTML renders them as text rather than as HTML tags or breaking the layout.

---

## Notes

- FR-124 can only be fully tested on a Linux machine running kernel 4.20+ with `CONFIG_PSI=y`. On macOS, T1a (absence of chart) is the only applicable test.
- FR-126 is fully testable on both platforms.
