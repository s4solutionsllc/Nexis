# FR-129 Plan — Design System Alignment

## Tasks

### Phase 1 — Font: Bundle Inter SemiBold (FR-129g)

- [ ] **1.1** Download `Inter-SemiBold.ttf` from the Inter v4 release (rsms.me/inter, OFL 1.1 licensed) and place it at `shared/nexis/static/font/Inter-SemiBold.ttf`
- [ ] **1.2** Add `<file>static/font/Inter-SemiBold.ttf</file>` to `shared/nexis/static.qrc` after Inter-Bold
- [ ] **1.3** Add `QFontDatabase::addApplicationFont(":/static/font/Inter-SemiBold.ttf");` to `shared/nexis/main.cpp` after the Inter-Bold registration (line ~338)
- [ ] **1.4** Build and confirm no errors; visually confirm nav active labels appear at SemiBold weight

**Acceptance:** `QFontDatabase::families()` includes `"Inter"` at weight 600. Nav item active text (`font-weight: 600`) renders visibly lighter than Bold (700).

---

### Phase 2 — Process Name Column Mono Font (FR-129h)

- [ ] **2.1** In `processes_page.cpp` `createRow()` (after `cmd_i` is constructed, ~line 311), add:
  ```cpp
  cmd_i->setFont(QFont(QStringLiteral("JetBrains Mono")));
  ```
- [ ] **2.2** In `updateRow()` (after `setCell(18, ...)` ~line 374), add:
  ```cpp
  if (auto *item = mItemModel->item(row, 18))
      item->setFont(QFont(QStringLiteral("JetBrains Mono")));
  ```
- [ ] **2.3** Incremental build; verify no regressions
- [ ] **2.4** Regenerate screenshot baselines (`NEXIS_GENERATE_REFS=1 ./test-ScreenshotTests`) — `processes` will change

**Acceptance:** Process names in the "Process" column render in JetBrains Mono; all other columns remain in the global font.

---

### Phase 3 — Service Description Mono Font (FR-129j)

- [ ] **3.1** In `style.qss`, add `font-family: @monoFontFamily;` to `#ServiceItem #lblServiceDescription` rule (~line 1270)
- [ ] **3.2** Incremental build; regenerate `services` baselines

**Acceptance:** Service description text (secondary line under service name) renders in JetBrains Mono.

---

### Phase 4 — Logo SVG Font Family (FR-129k)

- [ ] **4.1** In `shared/nexis/static/themes/default/img/sidebar-icons/sidebar-logo.svg`, change `font-family="Helvetica Neue, Arial, sans-serif"` → `font-family="Inter, Helvetica Neue, Arial, sans-serif"`
- [ ] **4.2** Same change in `shared/nexis/static/themes/default/img/sidebar-icons/sidebar-logo-collapsed.svg`
- [ ] **4.3** Same change in `shared/nexis/static/themes/light/img/sidebar-icons/sidebar-logo.svg`
- [ ] **4.4** Same change in `shared/nexis/static/themes/light/img/sidebar-icons/sidebar-logo-collapsed.svg`
- [ ] **4.5** Incremental build; visually confirm wordmark renders in Inter

**Acceptance:** Sidebar NEXIS wordmark uses Inter typeface on both themes (same letterforms as the rest of the UI).

---

### Phase 5 — Process Table Font Size (FR-129l)

- [ ] **5.1** In `style.qss`, in the `QTableView::item` block, add after it a targeted override:
  ```qss
  #tableProcess::item {
      font-size: 9pt;
  }
  ```
- [ ] **5.2** Incremental build; regenerate `processes` baseline

**Acceptance:** Process table rows render at 9pt (matching the design spec's 12px / ~9pt table density). Hardware Info table and other QTableView instances remain at 10pt.

---

### Phase 6 — Command Palette Keyboard Hint Footer (FR-129i)

- [ ] **6.1** In `command_palette.h`, add private member:
  ```cpp
  QWidget *mFooter = nullptr;
  ```
- [ ] **6.2** In `command_palette.cpp` `buildLayout()`, after `innerLayout->addWidget(mResultsList)`, add:
  ```cpp
  mFooter = new QWidget(container);
  mFooter->setObjectName("commandPaletteFooter");
  auto *footLayout = new QHBoxLayout(mFooter);
  footLayout->setContentsMargins(16, 6, 16, 6);
  footLayout->setSpacing(16);
  auto makeHint = [&](const QString &keys, const QString &label) {
      auto *w = new QWidget(mFooter);
      auto *h = new QHBoxLayout(w);
      h->setContentsMargins(0, 0, 0, 0);
      h->setSpacing(4);
      auto *k = new QLabel(keys, w);
      k->setObjectName("commandPaletteKey");
      auto *l = new QLabel(label, w);
      l->setObjectName("commandPaletteHintLabel");
      h->addWidget(k);
      h->addWidget(l);
      return w;
  };
  footLayout->addWidget(makeHint(QStringLiteral("\u2191\u2193"), tr("navigate")));
  footLayout->addWidget(makeHint(QStringLiteral("\u21b5"), tr("select")));
  footLayout->addWidget(makeHint(QStringLiteral("esc"), tr("close")));
  footLayout->addStretch();
  innerLayout->addWidget(mFooter);
  ```
- [ ] **6.3** Add QSS rules to `style.qss` for the footer:
  ```qss
  #commandPaletteFooter {
      border-top: 1px solid @borderColor;
      background-color: @color01;
  }
  
  #commandPaletteKey {
      font-family: @monoFontFamily;
      font-size: 8pt;
      color: @tertiaryText;
      padding: 1 4;
      border: 1px solid @borderColor;
      border-radius: 4;
      background-color: @color02;
  }
  
  #commandPaletteHintLabel {
      font-size: 8pt;
      color: @tertiaryText;
  }
  ```
- [ ] **6.4** Incremental build; open palette (Ctrl+K) and verify footer appears with correct styling
- [ ] **6.5** Run full test suite — no screenshot changes expected (palette isn't open during screenshot captures)

**Acceptance:** Command palette footer shows `↑↓ navigate  ↵ select  esc close` with keyboard glyphs styled in mono font with border, tertiary color. Footer matches design spec `.nx-palette-foot` intent.

---

### Phase 7 — Final Verification

- [ ] **7.1** Full build: `cmake --build build -j$(sysctl -n hw.ncpu)`
- [ ] **7.2** Full test suite: `ctest --test-dir build --output-on-failure`
- [ ] **7.3** Mark FR-129 `[x]` in `FEATURE_REQUESTS.md` with commit hash
- [ ] **7.4** Commit: `style(design-system): complete FR-129 design alignment (FR-129g–l)`
- [ ] **7.5** Move `FR-129_research.md` and `FR-129_plan.md` to `backlog/Archive/`

---

## Build Verification Steps

After each phase:
```bash
cmake --build build -j$(sysctl -n hw.ncpu) 2>&1 | tail -5
```

After phases that affect rendering (2, 3, 5):
```bash
NEXIS_GENERATE_REFS=1 ./build/output/test-ScreenshotTests
```

Full suite at end:
```bash
ctest --test-dir build --output-on-failure
```

## Rollback Notes

All changes are additive (new font file, QSS rules, a small widget in command_palette). If the Inter SemiBold download fails or the font is unavailable, skip Phase 1 — the remaining phases are independent. The QSS `font-weight: 600` rules already existed and will continue falling back to 700 gracefully.
