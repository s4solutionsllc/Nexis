# Resources — prototype rationale

**Status:** Approved by maintainer (2026-07-15).

**Before:** current-state/macos/dark/resources.png (and light/resources.png)
**After:** renders/resources_{dark,light}.png

**Changes (only the three approved dimensions — shadow depth, surface hierarchy, layout consistency):**
1. Wrapped each bare edge-to-edge history-chart band ("History of CPU", "History of CPU Load Averages", "History of GPU", "History of Disk Read Write") in its own DS §2 elevated container card (fill `@cardBgElevated`, 1px `@borderColor`, radius 12, single drop shadow alpha 90 / blur 26 / offset 0,2, 8px outer margin). The shadow lives on the container, never on chart internals. Source: DESIGN_SYSTEM.md DS §2 (`dashboard_tile_wrapper.cpp:107-130`, `addDropShadow(90,26)`); DS §7 (container-level shadow). Evidence: renders/resources_dark.png.
2. Established the three-layer surface hierarchy so each chart reads as a raised surface: page `@pageContent`, container `@cardBgElevated`, and the plot inset uses the chart surface token `@chartBackgroundColor` with `@chartGridColor` gridlines. In the before, the chart bands and the page were near-identical tones. Source: DS §1 (`values.ini:2,29,30`); DS §6 chart surface/grid tokens (`values.ini:60-79`, `CHANGELOG.md:31,45`).
3. Rebuilt each chart header on header anatomy: a 3px accent bar + the exact chart title + the "expand" (fullscreen) button kept top-right as an unchanged control. Accent-bar color is the metric token for that chart — CPU/Load `@cpuColor`, GPU `@gpuColor`, Disk `@diskColor`. Source: DS §3 (`metric_tile_base.cpp:255-307`, gear/action button `metric_tile_base.cpp:171-180`); DS §4 typography (`style.qss:772-815`).
4. Rendered the plots as static SVG (no JS chart library, no animation): `@chartBackgroundColor` surface, `@chartGridColor` gridlines, `@chartLabelColor` axis text. Axis labels copied exactly from the capture — CPU/GPU Y `100.0 / 75.0 / 50.0 / 25.0 / 0.0`, Load Averages Y `1.00 / 0.75 / 0.50 / 0.25 / 0.00`, all X axes `60.0 / 45.0 / 30.0 / 15.0 / 0.0`. Plots are empty (no data series drawn), matching the captured empty plots — no data was invented. Source: DS §6 (chart tokens); DS §9-2 (no animation, static chrome only).
5. Legend swatch rows copied exactly from the capture using the DS §6 series palette: CPU = 10 swatches (`@chartSeries01`–`@chartSeries10`), CPU Load Averages = 3 (`@chartSeries01`–`03`), GPU = 1 (`@chartSeries01`), Disk Read Write = 2 (`@chartSeries01`–`02`). Source: DS §6 series palette (`values.ini:60-79`).
6. Corrected the sidebar highlight to "Resources" (the committed harness capture shows a stale "Dashboard" highlight — CAPTURE_NOTES.md gotcha #5). Source: CAPTURE_NOTES.md §"Gotchas" item 5.

**Explicitly unchanged:** sidebar contents/order; the four chart titles and their order; each chart's legend swatch count/colors; the per-chart "expand" control; the axis labels and units. No data curves/bars were added to the empty captured plots. The "History of Disk Read Write" card's axis labels were below the fold in the capture, so only its visible chrome (title + 2 swatches + empty plot surface) is reproduced — no axis numerics were invented for it.
