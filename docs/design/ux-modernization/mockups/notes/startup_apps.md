# Startup Apps — prototype rationale

**Before:** current-state/macos/dark/startup_apps.png
**After:** renders/startup_apps_{dark,light}.png

**Changes:**
1. Wrapped the per-item startup cards in one elevated container card (DS §2 recipe) with flat `@cardBg` rows inside — one container-level shadow instead of a shadow on every row. Source: DS §2 (`dashboard_tile_wrapper.cpp:107-130`) + DS §7 (container-level shadow). Evidence: renders/startup_apps_dark.png.
2. Three-layer surface hierarchy (`@pageContent` / `@cardBgElevated` container / `@cardBg` rows). Source: DS §1 (`values.ini:2,29,30`).
3. Toolbar rebuilt on header anatomy: 3px accent bar + "Startup Applications" title + muted "22 items" source line (captured "(22)" count moved to the source line), keeping the real Search field, the "Add Startup App" primary action, and the "Repair BTM…" action (rendered on the danger button token to match its captured red treatment). Source: DS §3 (`metric_tile_base.cpp:255-307`); DS §4 (`style.qss:772-815`).
4. Preserved the captured "User Agents" / "System Agents" section grouping as muted uppercase group labels between the flat rows. Each row keeps its two-line layout — app name at the 9pt/600 title role, the `.plist` path at the muted meta role — and its three affordances unchanged: edit (pencil), remove (X), and the green enable toggle. Source: DS §4 typography (`style.qss:772-815`); DS §7 (`style.qss:328-350`); GNOME grouped-list convention [GNOME-HIG] (sources.md).

**Explicitly unchanged:** sidebar contents/order (Startup Apps highlighted); the Search / Add Startup App / Repair BTM toolbar controls; the User Agents / System Agents grouping; the per-row edit / remove / toggle affordances; every app name and LaunchAgents path copied from the capture; app-icon placeholders shown only on the rows that showed an icon in the capture. This page's capture is populated, so no empty state is added.
