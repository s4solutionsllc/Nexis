# Hardware Info — prototype rationale

**Before:** current-state/macos/dark/hardware_info.png
**After:** renders/hardware_info_{dark,light}.png

**Changes:**
1. Put each section body (System, Processor, Graphics, Memory) in its own elevated card (DS §2 recipe: `@cardBgElevated`, 1px `@borderColor`, radius 12, one drop shadow alpha 90 / blur 26 / offset 0,2, 8px margin). In the before, the sections were near-flat outlined boxes on a same-tone page; they now read as raised surfaces. Source: DS §2 (`dashboard_tile_wrapper.cpp:107-130`). Evidence: renders/hardware_info_dark.png.
2. Three-layer surface hierarchy (`@pageContent` base / `@cardBgElevated` cards) so the elevated cards separate from the page. Source: DS §1 (`values.ini:2,29,30`).
3. Gave each section title header anatomy — a 3px accent bar + label — instead of a plain floating heading, for cross-page layout consistency with the tile-header language. The Graphics section's accent bar uses the GPU metric token (`var(--gpuColor)`), while other sections use their respective type-appropriate accent tokens (System/Processor/Memory). Source: DS §3 (`metric_tile_base.cpp:255-307`); DS §3 accent bar rule ("accent bar takes a type-appropriate metric color, as on Dashboard tiles").
4. Typography consistency inside the key/value rows: labels 9pt/600 `@color06`, values `@color05`, with a `@borderColor` divider between rows. Source: DS §4 (`style.qss:772-815`).

**Explicitly unchanged:** sidebar; the section order (System → Processor → Graphics → Memory → Battery); every key and value copied verbatim from the capture; the full-width "Copy GPU Diagnostics" button. Battery is shown as a section title only — the capture cuts off there and no Battery rows are visible, so none were invented.
