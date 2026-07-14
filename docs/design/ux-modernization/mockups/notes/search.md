# Search — prototype rationale

**Before:** current-state/macos/dark/search.png
**After:** renders/search_{dark,light}.png

**Changes:**
1. Wrapped the bare results table in one elevated container card (DS §2 recipe: `@cardBgElevated`, 1px `@borderColor`, radius 12, one drop shadow alpha 90 / blur 26 / offset 0,2, 8px margin) — container-level shadow, flat rows. Source: DS §2 (`dashboard_tile_wrapper.cpp:107-130`) + DS §7. Evidence: renders/search_dark.png.
2. Applied the three-layer surface hierarchy (`@pageContent` / `@cardBgElevated` / `@cardBg`) so the results surface separates from the page. Source: DS §1 (`values.ini:2,29,30`).
3. This page has no page title in the current state, so header anatomy is applied as toolbar chrome only — consistent 14/12 top spacing and header-row gap 8, with the real controls kept (Browse… button, Search field, the "All" scope select, the primary search-icon button, and the "Advanced Search ▾" toggle). No title/accent bar was invented. Source: DS §3 (`metric_tile_base.cpp:255-307`); DS §4 (`style.qss:772-815`).
4. Frozen sticky header + tabular-numeric alignment on Size; zebra + hover recipe ready for result rows. Source: DS §7 (`style.qss:328-350`); [NNG-TABLES].
5. The capture shows an empty results table. Upgraded the bare-empty region to a full empty state: search icon + "No results yet" + a hint pointing at the query field / Browse… as the next action. Source: DS §5 + [NNG-EMPTY].

**Explicitly unchanged:** sidebar; the Browse… / Search / All-scope / search-button control row; the "Advanced Search ▾" toggle; the column set (Name, Path, Size, User, Creation Time); no invented rows.
