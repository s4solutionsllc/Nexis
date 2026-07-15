# Helpers — prototype rationale

**Before:** current-state/macos/dark/helpers.png
**After:** renders/helpers_{dark,light}.png

**Structure:** this page is a tool hub — a "Tools" button row + a "Maintenance" action-card grid — that swaps in a per-tool widget below. Per the task brief the prototype shows the hub grid plus the ONE representative tool widget visible in the capture: Host Manage.

**Changes:**
1. Recast the four Maintenance actions as DS §2 elevated action cards (fill `@cardBgElevated`, 1px `@borderColor`, radius 12, container-level shadow) in a 4-up grid; the Host Manage table sits in one DS §2 elevated container with flat `@cardBg` rows. Source: DS §2 (`dashboard_tile_wrapper.cpp:107-130`) + DS §7 (container-level shadow, flat rows). Evidence: renders/helpers_dark.png.
2. Three-layer surface hierarchy — page `@pageContent`, cards/containers `@cardBgElevated`, table rows `@cardBg`. Source: DS §1 (`values.ini:2,29,30`).
3. The "Tools" and "Maintenance" group labels use the DS §4 meta role (`@tertiaryText`, uppercase); each Maintenance card is title (DS §4 title role, `@color05`) over description (DS §4 meta role, `@tertiaryText`); the "Hosts (1)" sub-section leads with a 3px accent bar per DS §3. The captured tool buttons are kept, Host Manage active (`@accentColor`). Source: DS §3 (`metric_tile_base.cpp:255-307`); DS §4 (`style.qss:772-815`).
4. Frozen sticky header for the hosts table (IP Address / Full Qualified / Aliases) [NNG-TABLES]; the captured "New Host" and bottom "Save Changes" primary actions are kept on DS §3 spacing. Source: DS §7 (`style.qss:328-350`); [NNG-TABLES] (sources.md).

**Font note:** the host table cells (`127.0.0.1`, `localhost`) use the fixed monospace stack `"JetBrains Mono", ui-monospace, monospace` (`@monoFontFamily` QSS string, not a `values.ini` token). Source: `app_manager.cpp:208-209`.

**Explicitly unchanged:** sidebar contents/order (Helpers highlighted — the correct page highlight, not the stale "Dashboard" one, per CAPTURE_NOTES.md gotcha #5); the six Tools buttons (Host Manage active, Network Diagnostics, Open Ports, Firewall, SSD TRIM, Wake-on-LAN) and their order; the four Maintenance cards and their titles/descriptions; the Host Manage widget's "Hosts (1)" heading, "New Host" button, table columns (IP Address / Full Qualified / Aliases), and "Save Changes" action. The single `127.0.0.1 / localhost` row is a representative host matching the captured "Hosts (1)" count. The other five tool widgets (Network Diagnostics etc.) are not shown, per the "one representative widget" scope.
