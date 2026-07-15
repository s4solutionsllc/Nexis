# Services — prototype rationale

**Before:** current-state/macos/dark/services.png
**After:** renders/services_{dark,light}.png

**Changes:**
1. Collapsed the stack of individually-shadowed service cards into one elevated container card (fill `@cardBgElevated`, 1px `@borderColor`, radius 12, single drop shadow alpha 90 / blur 26 / offset 0,2, 8px outer margin) holding flat `@cardBg` rows. This supersedes the per-row `Utilities::addDropShadow(this, 30, 10)` each `ServiceItem` currently applies. Source: DS §2 (`dashboard_tile_wrapper.cpp:107-130`) + DS §7 (shadow on the container, never per row — supersedes `service_item.cpp:30`). Evidence: renders/services_dark.png.
2. Established the three-layer surface hierarchy (`@pageContent` page / `@cardBgElevated` container / `@cardBg` rows) so the list reads as one raised surface; in the before, each row floated on its own shadow with no unifying container. Source: DS §1 (`values.ini:2,29,30`).
3. Rebuilt the toolbar on header anatomy: a 3px accent bar + "System Services" title + a muted "27 services" source line (the captured "(27)" count moved into the source line), keeping the two real filter dropdowns ("Startup Status", "Running Status") and the two top-right start-all / stop-all icon actions. Source: DS §3 (`metric_tile_base.cpp:255-307`); DS §4 typography (`style.qss:772-815`).
4. Row typography follows the scale: service label at the 9pt/600 title role, the `– identifier` suffix at the muted meta role; long identifiers ellipsis-clip [NNG-TABLES] and rows carry a hover-highlight. Source: DS §4 (`style.qss:772-815`); DS §7 / [NNG-TABLES] (sources.md).
5. Status is shown on the DS §5 pill track (neutral `@chartGridColor` background, colored text): "Running" = `@successColor`, "Stopped" = `@tertiaryText` dimmed — replacing the before's solid-green / bordered-grey chips. The green on/off toggle affordance is kept unchanged. Source: DS §5 pill (`style.qss:809-815`) + `[status="…"]` selectors (`style.qss:2421-2435`).

**Explicitly unchanged:** sidebar contents/order (Services highlighted); the two filter dropdowns and the start-all/stop-all actions; the per-row green toggle; every service name, identifier, and running/stopped status copied verbatim from the capture (27 rows, first 13 visible); no invented rows or controls. This page's capture is populated, so no empty state is added.
