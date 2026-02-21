# Nexis UI/UX Redesign — Research Document

## 1. Current State Analysis

### 1.1 What Nexis Does Well

**Functional completeness.** Nexis packs 14 pages of system monitoring and optimization into a single window. Dashboard gauges, real-time resource charts, system cleaning, process management, service control, package management, Docker, and hardware inventory — it covers the full spectrum.

**Graceful degradation.** Conditional pages (Docker, GNOME Settings, Homebrew/APT) hide entirely when irrelevant. Dashboard gauges for battery, GPU, temperature, and disk health appear only when hardware is detected. This prevents confusion from grayed-out or "not available" entries.

**Consistent theming.** The token-based QSS system (`@accentColor`, `@cardBg`, etc.) makes theme changes trivial. Dark and light themes share identical layouts with only color values swapped.

**The Nexis brand identity.** The orange gradient wordmark (#FF4500 → #FF6B1A → #FF8C00 → #FFB347) against dark backgrounds is distinctive and memorable. The accent color (#E95420) carries through every interactive element.

### 1.2 Navigation Pain Points

| Issue | Description |
|-------|-------------|
| **Flat sidebar with 15 items** | Every page gets equal visual weight. Dashboard and Settings sit alongside niche pages like "Helpers" and "GNOME Settings." Users must scan the full list to find what they need. |
| **No grouping or hierarchy** | Monitoring pages (Dashboard, Resources, Hardware Info) are interleaved with management pages (System Cleaner, Services, Uninstaller). There's no visual separation by purpose. |
| **Sidebar eats 200px permanently** | On an 850px-wide window, the sidebar consumes 23% of horizontal space. All page content is squeezed into the remaining 650px. |
| **Page title wastes vertical space** | The centered bold "Dashboard" title bar occupies ~40px of vertical space on every page, duplicating information already visible in the selected sidebar button. |
| **No breadcrumbs or context** | Sub-pages (GNOME Settings tabs, Uninstaller tabs, System Cleaner scan results) use inline tab buttons that look different on every page. There's no consistent way to know "where you are" in a hierarchy. |
| **Feedback is not a page** | The "Feedback" button sits at the bottom of the sidebar styled like a page button, but it opens a dialog. This breaks the mental model. |

### 1.3 Dashboard Layout Issues

| Issue | Description |
|-------|-------------|
| **Gauge-only display** | The dashboard shows only current values (22% CPU, 5.0 GiB memory). There's no trend information — no sparklines, no "rising/falling" indicators, no historical context. |
| **Donut gauges are space-inefficient** | Each CircleBar occupies ~200x200px but communicates only two numbers (value + total). The partial-arc design (230-degree sweep) leaves large empty areas. |
| **No quick actions** | The dashboard is purely informational. To act on high disk usage, users must navigate to System Cleaner. To investigate high CPU, they must go to Processes. |
| **Rigid grid** | The 4-column grid doesn't adapt well. With all optional gauges showing, the second row has Temperature, GPU, Network bars, and Battery crammed into equal-width cells. |
| **Download/Upload bars are underwhelming** | The LineBar widgets (6px-tall progress bars) are the least readable elements on the dashboard. They show speed but not direction of change. |

### 1.4 Widget & Information Display Issues

| Issue | Description |
|-------|-------------|
| **Hardware Info is a wall of tables** | Stacked QGroupBox + QTableWidget sections create a long scrolling page with no visual hierarchy. System info, processor, graphics, memory, battery, and storage all look the same. |
| **Resources page is chart-only** | Seven full-width charts stacked vertically require extensive scrolling. There's no summary view or way to focus on one metric. |
| **Services page lacks visual status** | 86 services in a flat list with identical card styling. Running vs stopped services look the same until you check the toggle states. |
| **Processes table is generic** | The QTableView with sortable columns works but looks like every other process monitor. No visual emphasis on resource-hungry processes. |
| **System Cleaner's two-phase flow** | The category selection → scan results → clean flow works, but the transition between phases is jarring (stacked widget swap with no animation context). |

### 1.5 Brand Color Palette (Current)

```
Logo Gradient:     #FF4500 → #FF6B1A → #FF8C00 → #FFB347
App Accent:        #E95420 (Ubuntu Orange)
Accent Hover:      #c64516

Dark Theme:
  Background:      #222226
  Sidebar:         #2e2e32
  Cards:           #36363a
  Borders:         #5e5c64
  Primary Text:    #ffffff
  Secondary Text:  #9a9996

Light Theme:
  Background:      #fafafb
  Sidebar:         #ebebed
  Cards:           #ffffff
  Borders:         #deddda
  Primary Text:    #241f31
  Secondary Text:  #77767b

Semantic Colors:
  Success:         #2ec27e
  Warning:         #e5a50a
  Destructive:     #e01b24
```

---

## 2. Design Principles for the Redesign

### 2.1 Core Principles

1. **Monitor first, manage second.** The primary use case is checking system health. Management actions (cleaning, uninstalling, service control) are secondary. The UI should prioritize at-a-glance status.

2. **Progressive disclosure.** Show summary information by default. Let users drill down into detail on demand. Don't front-load complexity.

3. **Contextual actions.** When showing a metric (e.g., high disk usage), provide a direct path to the relevant action (e.g., "Clean" button) without requiring separate page navigation.

4. **Respect the viewport.** Maximize content area. Navigation should be compact and collapsible. Avoid fixed-size elements that waste space.

5. **Visual hierarchy through size, not just color.** The most important metrics should be physically larger. Secondary information should be smaller but still accessible.

---

## 3. Proposed Design Concepts

I'm proposing three distinct concepts. Each takes a different approach to navigation and information display while maintaining the Nexis Orange/Black/Cream color identity.

---

### Concept A: "Bento Dashboard" — Collapsible Sidebar + Bento Grid

**Navigation:** Collapsible sidebar (64px icons-only ↔ 220px expanded) with grouped sections.

**Key Ideas:**
- Sidebar collapses to icon-only rail, giving the content area an extra 156px
- Pages grouped into 3 sections with subtle dividers: **Monitor** (Dashboard, Hardware, Resources), **Manage** (Cleaner, Processes, Services, Apps, Startup), **System** (Docker, Helpers, Repositories, GNOME, Settings)
- Dashboard uses a **bento grid** layout — asymmetric cards of varying sizes
- Hero card (CPU + Memory combined) takes 2x width
- Each card combines a gauge + sparkline + trend indicator
- Quick-action chips on cards (e.g., "View Processes" on CPU card, "Clean" on Disk card)
- Page title moves into sidebar header area (no separate title bar)
- Command palette (Ctrl+K) for quick navigation and search

**Dashboard Bento Layout:**
```
┌──────────────────────┬───────────┬───────────┐
│                      │           │           │
│   CPU + Memory       │   Disk    │  Network  │
│   (Hero Card)        │  Usage    │  Speed    │
│   Gauges + Spark     │  Donut    │  Sparkline│
│                      │           │           │
├───────────┬──────────┼───────────┼───────────┤
│           │          │           │           │
│   GPU     │  Temp    │  Battery  │  Disk     │
│   Usage   │  Sensor  │  Health   │  Health   │
│           │          │           │           │
├───────────┴──────────┴───────────┴───────────┤
│  Quick Actions: [Clean System] [View Procs]  │
│  [Check Updates]  Recent: Cleaned 2.3 GB ago │
└──────────────────────────────────────────────┘
```

**Strengths:** Modern bento aesthetic. Collapsible sidebar maximizes content. Grouped navigation reduces cognitive load. Quick actions reduce page-hopping.

**Tradeoffs:** Bento layout is more complex to implement in QGridLayout. Variable card sizes require careful responsive handling.

---

### Concept B: "Top-Bar Navigator" — Horizontal Navigation + Dashboard Hub

**Navigation:** Top navigation bar with dropdown menus, replacing the sidebar entirely.

**Key Ideas:**
- Horizontal nav bar below the title bar: **Dashboard** | **Hardware** ▾ | **Tools** ▾ | **Settings**
- "Hardware" dropdown: Hardware Info, Resources
- "Tools" dropdown: System Cleaner, Processes, Services, Startup Apps, Uninstaller, Helpers, Docker, Repositories, GNOME Settings
- Full window width available for content (no sidebar eating 200px)
- Dashboard becomes a **hub page** with summary cards that link to detail pages
- Each summary card shows the key metric + a mini-chart + a "Details →" link
- Hardware Info redesigned as a **card grid** instead of stacked tables
- Resources page gets a **tab strip** to show one chart at a time (instead of all 7 stacked)
- Breadcrumb trail appears below nav bar when in sub-pages

**Top Bar Layout:**
```
┌─────────────────────────────────────────────┐
│  [N] Nexis    Dashboard  Hardware▾  Tools▾  ⚙│
├─────────────────────────────────────────────┤
│                                             │
│  ┌─────────┐ ┌─────────┐ ┌─────────┐      │
│  │  CPU    │ │ Memory  │ │  Disk   │      │
│  │  ████   │ │  ████   │ │  ████   │      │
│  │  22%    │ │  16%    │ │  9%     │      │
│  │ ~~~~~~~~│ │ ~~~~~~~~│ │ ~~~~~~~~│      │
│  │ Details→│ │ Details→│ │ Details→│      │
│  └─────────┘ └─────────┘ └─────────┘      │
│                                             │
│  ┌─────────┐ ┌─────────┐ ┌─────────┐      │
│  │  GPU    │ │  Temp   │ │ Network │      │
│  │  ████   │ │  30°C   │ │ ↓28 KB/s│      │
│  │  0%     │ │ ~~~~~~~~│ │ ~~~~~~~~│      │
│  │ Details→│ │ Details→│ │ Details→│      │
│  └─────────┘ └─────────┘ └─────────┘      │
│                                             │
│  ┌─────────────────────────────────────┐   │
│  │  System Health Summary              │   │
│  │  ✓ All drives healthy  ⚡ Battery OK │   │
│  │  [Clean System: 2.1 GB reclaimable] │   │
│  └─────────────────────────────────────┘   │
│                                             │
└─────────────────────────────────────────────┘
```

**Strengths:** Maximum content width. Clean, minimal chrome. Familiar web-app navigation pattern. Hub dashboard provides overview + navigation in one.

**Tradeoffs:** Dropdown menus in Qt require careful styling. 14 pages in a top bar means some items must be nested. Less discoverable than a visible sidebar.

---

### Concept C: "Adaptive Sidebar" — Icon Rail + Contextual Panel

**Navigation:** Narrow icon rail (48px) permanently visible on left, with a contextual detail panel that slides out.

**Key Ideas:**
- **Icon rail**: 48px wide, always visible. Shows 6-8 category icons (Dashboard, Hardware, Tools, Resources, Docker, Settings). Each icon is a category, not a specific page.
- **Contextual panel**: Clicking a category icon slides out a 200px panel showing the pages within that category. Clicking a page loads it. Clicking elsewhere or clicking the same category icon closes the panel.
- This gives the content area maximum width (full width minus 48px) most of the time
- Dashboard gets a **unified metrics view**: All gauges arranged in a responsive flow layout, each gauge being a compact "metric tile" with number + mini-visualization + label
- Hardware Info uses **accordion sections** instead of scrollable group boxes — one section open at a time
- Resources page uses a **tab bar** with mini sparkline previews on each tab
- System Cleaner shows a **progress ring** in the icon rail badge when cleaning is active
- Notification badges on icon rail categories (e.g., red dot on "Tools" when updates available)

**Icon Rail + Panel Layout:**
```
┌──┬──────────────────────────────────────────┐
│  │                                          │
│🏠│  ┌─────┐ ┌─────┐ ┌─────┐ ┌─────┐      │
│  │  │ CPU │ │ MEM │ │DISK │ │ NET │      │
│💻│  │ 22% │ │ 16% │ │ 9%  │ │28K/s│      │
│  │  │~~~~~│ │~~~~~│ │~~~~~│ │~~~~~│      │
│🔧│  └─────┘ └─────┘ └─────┘ └─────┘      │
│  │  ┌─────┐ ┌─────┐ ┌─────┐ ┌─────┐      │
│📊│  │ GPU │ │TEMP │ │ BAT │ │HLTH │      │
│  │  │ 0%  │ │30°C │ │ 85% │ │Good │      │
│🐳│  │~~~~~│ │~~~~~│ │~~~~~│ │~~~~~│      │
│  │  └─────┘ └─────┘ └─────┘ └─────┘      │
│⚙ │                                          │
│  │  ┌──────────────────────────────────┐   │
│  │  │ System Status: All Clear          │   │
│  │  │ Last Clean: 2 hrs ago (2.3 GB)   │   │
│  │  │ [Quick Clean] [View Resources →] │   │
│  │  └──────────────────────────────────┘   │
│  │                                          │
└──┴──────────────────────────────────────────┘

When "Tools" (🔧) icon clicked:
┌──┬─────────────┬───────────────────────────┐
│  │ Tools       │                           │
│🏠│             │  (current page content    │
│  │ ▸ Cleaner   │   remains visible but     │
│💻│ ▸ Processes  │   dimmed/shifted)         │
│  │ ▸ Services   │                           │
│🔧│ ▸ Startup   │                           │
│  │ ▸ Uninstaller│                           │
│📊│ ▸ Helpers    │                           │
│  │             │                           │
│🐳│             │                           │
│  │             │                           │
│⚙ │             │                           │
└──┴─────────────┴───────────────────────────┘
```

**Strengths:** Most space-efficient. Icon rail is immediately recognizable. Category grouping is enforced. Badge notifications are natural on icon rail. Feels modern (similar to VS Code, Slack, Discord sidebar patterns).

**Tradeoffs:** Two-step navigation (click category, then page) adds a click. Fly-out panel needs careful animation to feel responsive. Users may not discover pages hidden behind category icons.

---

## 4. Dashboard Widget Redesign Proposals

Regardless of which navigation concept is chosen, the dashboard widgets themselves can be significantly improved.

### 4.1 Metric Tile (Replaces CircleBar)

Instead of the current large donut gauge, each metric becomes a compact rectangular tile:

```
┌────────────────────────┐
│  CPU                   │
│  ██████████░░░░ 22%    │  ← Arc or linear gauge
│  ~~~~~~~~~~~~~~~~~~~~~~~~│  ← 60-second sparkline
│  2.83 GHz  ↑ trending  │  ← Value + trend arrow
└────────────────────────┘
```

**Benefits:**
- Shows current value, historical trend, and direction in one compact widget
- Rectangular tiles pack more efficiently than circles
- Sparkline provides 60 seconds of context (reusing HistoryChart data)
- Trend arrow (↑↓→) immediately shows if a metric is rising, falling, or stable

### 4.2 Network Tile (Replaces LineBar)

```
┌────────────────────────┐
│  Network               │
│  ↓ 28.7 KB/s  ↑ 8.0 KB│
│  ┌────────────────────┐│
│  │  ~~download~~~~    ││  ← Dual sparkline
│  │  --upload------    ││
│  └────────────────────┘│
│  Total: ↓9.2 GB ↑25.8G │
└────────────────────────┘
```

### 4.3 Disk Health Tile (New combined widget)

```
┌────────────────────────┐
│  Storage               │
│  ▰▰▰▰▰▰▱▱▱▱  83.8/937 │
│  NVMe 1: Good ● 30°C  │
│  NVMe 2: Good ● 28°C  │
│  [Clean: ~2.1 GB free] │
└────────────────────────┘
```

### 4.4 System Summary Card (New)

```
┌──────────────────────────────────────────┐
│  System: media • Ubuntu 24.04 • Ryzen 7 │
│  Uptime: 3d 14h • Updates: 12 available │
│  Last Clean: 2h ago • Next: Tomorrow 3AM│
└──────────────────────────────────────────┘
```

---

## 5. Color Palette Refinements

The current palette is solid but can be refined to better match the Nexis logo gradient while maintaining the Orange/Black/Cream identity.

### 5.1 Proposed Dark Theme Palette

```
Brand Colors (from logo):
  Orange Primary:    #FF6B1A  (logo gradient 30%, more vibrant than #E95420)
  Orange Dark:       #E95420  (keep as hover/pressed state)
  Orange Light:      #FFB347  (accent highlights, badges)
  Orange Glow:       rgba(255, 107, 26, 0.15)  (subtle card highlights)

Surfaces:
  Base:              #1A1C22  (darker than current #222226, matches logo dark bg)
  Sidebar/Rail:      #222228  (slightly lighter for separation)
  Card:              #2A2C32  (between current sidebar and card colors)
  Card Elevated:     #32343A  (hover states, active cards)
  Card Border:       #3A3D4A  (matches logo dark bg border)

Text:
  Primary:           #F0F2F5  (slightly warm white, not pure #ffffff)
  Secondary:         #9A9DA6  (slightly cooler than current #9a9996)
  Tertiary:          #6B6E78  (for timestamps, metadata)
  On-Accent:         #FFFFFF  (pure white on orange backgrounds)

Semantic:
  Success:           #2EC27E  (keep)
  Warning:           #FFB347  (use logo orange-light instead of #e5a50a)
  Destructive:       #E05454  (softer red than #e01b24)
  Info:              #5B9BD5  (blue for informational)
```

### 5.2 Proposed Light Theme Palette

```
Surfaces:
  Base:              #F5F0EB  (warm cream instead of cool #fafafb)
  Sidebar/Rail:      #EDE7E0  (warm cream darker)
  Card:              #FFFFFF  (keep white cards)
  Card Elevated:     #FFF8F2  (very light orange tint on hover)

Text:
  Primary:           #1A1C22  (match dark theme base)
  Secondary:         #6B6E78  (match dark tertiary)
  Tertiary:          #9A9DA6  (match dark secondary)

Accent:
  Orange Primary:    #E95420  (slightly darker for light bg readability)
  Orange Hover:      #C64516  (keep)
```

### 5.3 Gauge Color Assignments

Each system metric gets a consistent color identity:

| Metric | Color | Hex | Reasoning |
|--------|-------|-----|-----------|
| CPU | Nexis Orange | #FF6B1A | Brand hero color for the primary metric |
| Memory | Warm Amber | #FFB347 | Logo gradient end color — warm, related to orange |
| Disk | Coral Red | #E05454 | "Storage full" naturally maps to warning/red |
| Network | Teal | #26A69A | Cool contrast against warm palette |
| GPU | Purple | #813D9C | Distinct, associated with graphics/gaming |
| Temperature | Blue | #5B9BD5 | "Cool/hot" naturally maps to blue spectrum |
| Battery | Green | #2EC27E | Green = charged, healthy |
| Disk Health | Orange Glow | #FF8C00 | Logo gradient midpoint |

---

## 6. Hardware Info Redesign

The current Hardware Info page is a long scroll of QGroupBox + QTableWidget sections. Proposed redesign:

### Option 1: Card Grid

```
┌───────────────────────────────────────────────┐
│  Hardware Info                                 │
├───────────────┬───────────────┬───────────────┤
│  System       │  Processor    │  Graphics     │
│  ┌─────────┐  │  ┌─────────┐  │  ┌─────────┐  │
│  │ Ubuntu  │  │  │ Ryzen 7 │  │  │ NVIDIA  │  │
│  │ 24.04   │  │  │ 5700X   │  │  │ GPU 1   │  │
│  │ x86_64  │  │  │ 8C/16T  │  │  │         │  │
│  │ 6.17.0  │  │  │ 4.67GHz │  │  │         │  │
│  └─────────┘  │  └─────────┘  │  └─────────┘  │
├───────────────┼───────────────┼───────────────┤
│  Memory       │  Storage      │  Battery      │
│  ┌─────────┐  │  ┌─────────┐  │  ┌─────────┐  │
│  │ 31.2 GB │  │  │ NVMe 1  │  │  │ 85%     │  │
│  │ DDR4    │  │  │ 937 GB  │  │  │ Healthy │  │
│  │ 8GB Swap│  │  │ Good ●  │  │  │ 523 cyc │  │
│  └─────────┘  │  └─────────┘  │  └─────────┘  │
└───────────────┴───────────────┴───────────────┘
  Each card expands on click to show full detail table
```

### Option 2: Accordion (one section expanded at a time)

```
┌───────────────────────────────────────────┐
│  ▾ System                                 │
│    Hostname    media                      │
│    Platform    linux_x86_64               │
│    Distribution Ubuntu 24.04.4 LTS        │
│    Kernel      6.17.0-14-generic          │
│    ...                                    │
├───────────────────────────────────────────┤
│  ▸ Processor                              │
├───────────────────────────────────────────┤
│  ▸ Graphics                               │
├───────────────────────────────────────────┤
│  ▸ Memory                                 │
├───────────────────────────────────────────┤
│  ▸ Storage                                │
├───────────────────────────────────────────┤
│  ▸ Battery                                │
└───────────────────────────────────────────┘
```

---

## 7. Resources Page Redesign

### Current Problem
Seven full-width charts stacked vertically. Must scroll to see all. No way to focus on one metric.

### Proposed: Tabbed Chart Viewer with Preview Strip

```
┌──────────────────────────────────────────────┐
│  [CPU] [Load] [GPU] [Disk] [Mem] [Net] [Tmp]│  ← Tab strip
│   ~~~   ~~~   ~~~    ~~~    ~~~   ~~~   ~~~  │  ← Mini sparkline previews
├──────────────────────────────────────────────┤
│                                              │
│         Large Chart View (selected tab)      │
│                                              │
│    100 ┤                                     │
│     75 ┤          ╭─╮                        │
│     50 ┤    ╭─────╯ ╰──╮                    │
│     25 ┤────╯           ╰──────────          │
│      0 ┤                                     │
│        └──────────────────────────────       │
│         60s            30s            0s     │
│                                              │
│  Legend: ● Core 0  ● Core 1  ● Core 2  ...  │
│                                              │
│  ┌─────────────────────────────────────┐     │
│  │ Summary: Avg 22% • Peak 67% (12s ago)│    │
│  │ Load: 0.25 / 0.72 / 0.50            │    │
│  └─────────────────────────────────────┘     │
│                                              │
│  [Expand All Charts ↓]                       │
└──────────────────────────────────────────────┘
```

---

## 8. System Cleaner Redesign

### Current Flow
Phase 1 (category selection with large icons) → Scan button → Phase 2 (file tree with checkboxes)

### Proposed: Single-View with Progressive Disclosure

```
┌──────────────────────────────────────────────┐
│  System Cleaner                              │
│                                              │
│  ┌──────────┐ ┌──────────┐ ┌──────────┐    │
│  │ ✓ Pkgs  │ │ ✓ Crash  │ │ □ Logs   │    │
│  │ 340 MB  │ │ 12 MB    │ │ scan...  │    │  ← Category cards with
│  │ 23 files│ │ 4 files  │ │          │    │     live size estimates
│  └──────────┘ └──────────┘ └──────────┘    │
│  ┌──────────┐ ┌──────────┐ ┌──────────┐    │
│  │ □ Cache  │ │ ✓ Trash  │ │ □ DevTool│    │
│  │ scan...  │ │ 1.8 GB   │ │ scan...  │    │
│  │          │ │ 156 files│ │          │    │
│  └──────────┘ └──────────┘ └──────────┘    │
│                                              │
│  Total Selected: 2.15 GB (183 files)        │
│                                              │
│  ▾ Selected Files (expand to review)         │
│    ▾ Package Caches (340 MB)                 │
│      ✓ apt-cache/archives/ (280 MB)         │
│      ✓ snap/cache/ (60 MB)                  │
│    ▾ Crash Reports (12 MB)                   │
│      ✓ /var/crash/* (12 MB)                 │
│                                              │
│  [Cancel]                    [Clean 2.15 GB] │
│                                              │
│  ─── Schedule ──────────────────────────     │
│  Weekly clean: Enabled (Sun 3:00 AM)         │
│  Last run: Feb 18, cleaned 2.3 GB           │
└──────────────────────────────────────────────┘
```

---

## 9. Interaction & Animation Recommendations

### 9.1 Transitions
- **Page transitions**: Crossfade (200ms) instead of horizontal slide. Slides feel dated; fades feel instant.
- **Sidebar collapse**: Smooth width animation (250ms, ease-out)
- **Card hover**: Subtle elevation increase (shadow spread +2px, 150ms)
- **Gauge updates**: Animated value transitions (100ms, ease-in-out)

### 9.2 Micro-interactions
- **Sparkline drawing**: New data points animate in from the right edge
- **Trend arrows**: Fade-rotate when direction changes
- **Category badges**: Pulse animation when count changes
- **Clean progress**: Ring animation around the cleaner icon in the sidebar

### 9.3 Command Palette (Ctrl+K)
```
┌──────────────────────────────────────┐
│  🔍 Search pages, actions, settings... │
│                                      │
│  Recent                              │
│  ● Dashboard                         │
│  ● Resources → CPU                   │
│                                      │
│  Pages                               │
│  ● Hardware Info                     │
│  ● System Cleaner                    │
│  ● Processes                         │
│                                      │
│  Actions                             │
│  ▸ Quick Clean                       │
│  ▸ End Process...                    │
│  ▸ Toggle Theme                      │
│                                      │
│  Settings                            │
│  ▸ Alert Thresholds                  │
│  ▸ Language                          │
│  ▸ Color Scheme                      │
└──────────────────────────────────────┘
```

---

## 10. Recommendation

**I recommend Concept A (Bento Dashboard with Collapsible Sidebar)** as the primary direction, incorporating elements from Concept C (icon rail) as the collapsed state.

**Rationale:**
1. The collapsible sidebar is the industry standard for data-heavy desktop apps (VS Code, Slack, Discord, JetBrains IDEs all use this pattern).
2. The bento grid dashboard is the most visually striking upgrade and solves the "gauge-only, no context" problem.
3. Grouped sidebar sections solve the "15 undifferentiated items" problem without requiring the two-step navigation of Concept C.
4. The command palette (Ctrl+K) catches power users who want instant navigation.
5. It's the most incremental change from the current architecture — the sidebar still exists, pages still live in a stacked widget. The changes are mostly layout and styling, not fundamental restructuring.

**Implementation priority:**
1. Collapsible sidebar with grouped sections (high impact, moderate effort)
2. Dashboard bento grid with metric tiles + sparklines (high impact, high effort)
3. Command palette (moderate impact, moderate effort)
4. Resources tabbed view (moderate impact, low effort)
5. Hardware Info card grid (low impact, low effort)
6. System Cleaner single-view (low impact, moderate effort)

---

## 11. Mockup Images

See the following SVG mockup files in `claude_definitions/mockups/`:

1. `concept_a_dashboard_dark.svg` — Bento Dashboard with collapsible sidebar (dark theme)
2. `concept_a_dashboard_light.svg` — Same layout in light/cream theme
3. `concept_a_sidebar_collapsed.svg` — Dashboard with sidebar in collapsed icon-rail mode
4. `concept_b_topbar_dashboard.svg` — Top-bar navigation concept
5. `concept_c_iconrail.svg` — Icon rail with contextual panel
6. `metric_tile_designs.svg` — Detailed metric tile widget designs
7. `resources_tabbed.svg` — Redesigned Resources page
8. `hardware_cards.svg` — Redesigned Hardware Info page
