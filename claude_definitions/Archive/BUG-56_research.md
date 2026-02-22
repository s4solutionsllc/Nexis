# BUG-56 Research: Navbar items centered instead of left-aligned after expanding from collapsed state

## Bug Description

When the app is closed with the navbar collapsed and then reopened, the navbar correctly initializes in collapsed state. However, when the user expands the navbar, the nav item labels are **centered** instead of **left-aligned** as they should be.

## Root Cause

**Missing child widget re-polish after parent dynamic property change.**

The sidebar uses a QSS dynamic property `collapsed` on the `#sidebar` widget to toggle between two sets of stylesheet rules:

**Expanded state** (`style.qss:433-443`):
```qss
#sidebar QPushButton {
    text-align: left;      /* ← LEFT */
    padding: 0 @dp12;
    border-left: 3px solid transparent;
    ...
}
```

**Collapsed state** (`style.qss:490-495`):
```qss
#sidebar[collapsed="true"] QPushButton {
    text-align: center;    /* ← CENTER */
    padding: 0;
    border-left: 0;
    border-bottom: 3px solid transparent;
}
```

When expanding, `applySidebarCollapse()` at `app.cpp:708-710` does:
```cpp
ui->sidebar->setProperty("collapsed", collapsed);
ui->sidebar->style()->unpolish(ui->sidebar);
ui->sidebar->style()->polish(ui->sidebar);
```

**The problem:** `unpolish()`/`polish()` is only called on the **parent** `#sidebar` widget. In Qt, `QStyle::polish(QWidget*)` does **not** recursively re-polish child widgets. The child `QPushButton` widgets retain the cached style from `#sidebar[collapsed="true"] QPushButton` (center-aligned) because they were never told to re-evaluate their styles.

This is a well-known Qt behavior: when a parent widget's dynamic property changes, child selectors that depend on that property (e.g., `#parent[prop="x"] ChildWidget`) are not automatically re-evaluated. Each child must be explicitly unpolished/polished, or a full stylesheet re-application must be triggered.

## Why it only manifests after restart with collapsed state

1. **App starts** → `buildSidebar()` creates buttons → QSS applies `#sidebar QPushButton` → `text-align: left` ✓
2. **Settings restore** (`app.cpp:409-411`): `applySidebarCollapse(true, false)` sets `collapsed=true` → buttons styled with `text-align: center`
3. **User expands** → `applySidebarCollapse(false, true)` sets `collapsed=false` → only sidebar is re-polished → **child buttons keep `text-align: center`** ✗

If the app starts expanded, buttons never receive the centered style, so expanding/collapsing/expanding works fine on the first collapse cycle because the initial polish applied `text-align: left`.

## Code Flow Trace

### Startup restoration (`app.cpp:409-411`)
```cpp
if (SettingManager::ins()->getSidebarCollapsed())
    applySidebarCollapse(true, false);  // no animation
```

### Toggle trigger (`app.cpp:617-623`)
```cpp
void App::toggleSidebarCollapse() {
    mSidebarCollapsed = !mSidebarCollapsed;
    SettingManager::ins()->setSidebarCollapsed(mSidebarCollapsed);
    applySidebarCollapse(mSidebarCollapsed, true);  // with animation
    emit SignalMapper::ins()->sigSidebarCollapseToggled(mSidebarCollapsed);
}
```

### applySidebarCollapse (`app.cpp:625-711`)
Key operations in order:
1. **Lines 631-645**: Width animation (64 ↔ 220px, 250ms, OutCubic)
2. **Lines 652-657**: Toggle section headers/indicators visibility
3. **Lines 670-679**: Save/restore button text (clear text when collapsed, restore when expanded)
4. **Lines 691-698**: Update toggle button icon
5. **Lines 700-705**: Swap logo variant
6. **Lines 708-710**: Set `collapsed` property + `unpolish`/`polish` on sidebar only ← **BUG HERE**

### Affected QSS rules (`style.qss:433-514`)
The expanded rules at lines 433-456 use `#sidebar QPushButton` (no property selector).
The collapsed overrides at lines 490-514 use `#sidebar[collapsed="true"] QPushButton`.

When `collapsed` changes from `"true"` to `false`, the `#sidebar[collapsed="true"] QPushButton` selector should stop matching, and the base `#sidebar QPushButton { text-align: left }` should take effect. But since the child buttons aren't re-polished, the stale centered alignment persists.

## Files Involved

| File | Role |
|------|------|
| `shared/nexis/app.cpp:625-711` | `applySidebarCollapse()` — collapse/expand logic |
| `shared/nexis/app.cpp:409-411` | Startup state restoration |
| `shared/nexis/app.cpp:617-623` | Toggle trigger |
| `shared/nexis/app.cpp:48-58` | `createSidebarButton()` |
| `shared/nexis/app.h` | `mListSidebarButtons` member |
| `shared/nexis/static/themes/default/style/style.qss:433-514` | Sidebar QSS rules |
| `shared/nexis/Managers/setting_manager.cpp` | Persistence of sidebar state |

## Fix Direction

After setting the `collapsed` property and re-polishing the sidebar, iterate over all child buttons and re-polish each one so Qt re-evaluates the property-dependent QSS selectors. This is a minimal, targeted fix.
