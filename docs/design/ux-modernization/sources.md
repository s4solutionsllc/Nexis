# External Source Pack — UX Modernization

**Purpose:** This file is the citation pack for the Nexis UX modernization effort. It
defines the citation keys (e.g. `[QT-QSS]`, `[NNG-TABLES]`) referenced by
`docs/design/ux-modernization/DESIGN_SYSTEM.md` and by prototype/rationale notes
elsewhere under `docs/design/ux-modernization/`. Each key maps to one externally
verified source (framework docs or platform/UX guidance) that backs a specific
design claim.

**Append-only:** Do not edit or remove existing rows. New sources may only be
*appended* to the table below, and only when a later rationale note needs a claim
that isn't already covered by an existing key. Every appended row must go through
the same verification steps used here (fetch, confirm live, confirm the page
actually supports the claim, record title + access date).

## Sources

| Key | Claim it supports | URL | Title | Accessed |
|---|---|---|---|---|
| QT-QSS | QSS selector styling; prefer app-level stylesheet | https://doc.qt.io/qt-6/stylesheet.html | Qt Style Sheets \| Qt Widgets \| Qt 6.11.1 | 2026-07-14 |
| QT-QSS-SYNTAX | Property-selector re-polish requirement (BUG-56 basis) | https://doc.qt.io/qt-6/stylesheet-syntax.html | The Style Sheet Syntax \| Qt Widgets \| Qt 6.11.1 | 2026-07-14 |
| QT-SHADOW | Drop-shadow effect = pixmap render + blur, repaint cost | https://doc.qt.io/qt-6/qgraphicsdropshadoweffect.html | QGraphicsDropShadowEffect Class \| Qt Widgets \| Qt 6.11.1 | 2026-07-14 |
| HIG-MACOS | macOS typography hierarchy, materials/elevation, margins | https://developer.apple.com/design/human-interface-guidelines | Human Interface Guidelines \| Apple Developer Documentation | 2026-07-14 |
| GNOME-HIG | GNOME boxed lists, header patterns, spacing units | https://developer.gnome.org/hig/ | GNOME Human Interface Guidelines | 2026-07-14 |
| NNG-TABLES | Data-table density, alignment, zebra vs whitespace | https://www.nngroup.com/articles/data-tables/ | Data Tables: Four Major User Tasks | 2026-07-14 |
| NNG-EMPTY | Empty states explain + offer next action | https://www.nngroup.com/articles/empty-state-interface-design/ | Designing Empty States in Complex Applications: 3 Guidelines | 2026-07-14 |

## Supporting content found

- **QT-QSS** — The "Overview" section states style sheets can be set on the whole
  application via `QApplication::setStyleSheet()` or on a specific widget, with
  worked examples of type/ID/property selector syntax immediately below it.
- **QT-QSS-SYNTAX** — The "Selector Types" table's "Property Selector" row is
  followed by an explicit warning that if a Qt property's value changes after the
  stylesheet is set, the stylesheet may need to be forced to recompute (e.g. by
  unsetting and resetting it) — the documented basis for BUG-56's
  unpolish()/polish() workaround.
- **QT-SHADOW** — The "Detailed Description" and the `blurRadius`/`offset`/`color`
  property docs confirm the effect renders the source with a configurable drop
  shadow. Note: this page does not itself describe the pixmap-render/blur
  implementation or repaint cost — that part of the claim is an architectural
  inference from `QGraphicsEffect` (the base class `draw()` is reimplemented, not
  further documented here), not a verbatim statement on this page. No dead link
  or substitution was needed; this is simply the level of detail Qt publishes for
  this class.
- **HIG-MACOS** — The landing page's "Foundations of design" section links
  directly to Typography, Materials, Layout, Color, and Accessibility topics
  (confirmed by rendering the page, since Apple's HIG index is JS-driven and a
  plain fetch only returns the title). The linked Layout page includes a macOS
  "Platform considerations" subsection plus general guidance on margins, safe
  areas, and layout guides, backing the "margins" part of the claim.
- **GNOME-HIG** — The "Content Overview" section describes a "Patterns" section
  "organized into four types: containers, navigation, feedback, and controls."
  The linked Containers > Boxed Lists page gives detailed boxed-list guidelines
  (row types, spacing/width constraints), directly supporting the claim.
- **NNG-TABLES** — The "1. Locating Relevant Info" subsection under "Compare
  Data" explicitly recommends "Borders, zebra striping, and hover-triggered
  highlighting" plus frozen header rows/columns to help users keep their place
  while scanning — the zebra-vs-whitespace scanning-aid claim. The article does
  not use the word "density" or discuss numeric-column alignment directly; those
  are reasonable extrapolations from its broader table-scanning guidance rather
  than verbatim statements.
- **NNG-EMPTY** — The "Use Empty States to Provide Direct Pathways for Key
  Tasks" section recommends "brief yet explicit instructions" and next-action
  links, illustrated by an alerts panel with a "Create" button plus a
  "Learn more" link — directly supporting the explain-and-offer-next-action
  claim.
