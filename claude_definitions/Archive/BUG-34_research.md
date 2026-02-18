# BUG-34 Research: Settings page link color

## Problem

The "Luke Simpson" GitHub profile link on the Settings page uses hardcoded blue (`#007af4`) instead of the Nexis accent orange (`#E95420`).

## Affected Locations

### 1. `settings_page.cpp` (line 25–30)

Dynamic `setText()` in the constructor:
```cpp
ui->lblCreatedBy->setText(
    QString("<html><head/><body><p>Nexis v%1 "
            "<a href=\"https://github.com/lsimpsonsfdc\">"
            "<span style=\" text-decoration: underline; color:#007af4;\">"
            "Luke Simpson</span></a></p></body></html>")
        .arg(qApp->applicationVersion()));
```

The `.cpp` version overwrites the `.ui` default at runtime (it appends the app version).

### 2. `settings_page.ui` (line 236)

Fallback default text in the UI file:
```xml
<string notr="true">&lt;html&gt;...color:#007af4;...&lt;/string&gt;
```

This is overwritten by the `.cpp` constructor, but should be kept consistent.

## Theme Colors

From `shared/nexis/static/themes/default/style/values.ini`:
- `@accentColor=#E95420` — Nexis brand orange
- `@accentHover=#c64516` — Darker orange for hover

Since this is inline HTML inside a QLabel with `textFormat: Qt::RichText` and `openExternalLinks: true`, CSS hover pseudo-classes don't apply. The fix is simply to replace `#007af4` with `#E95420` in both locations.

## Fix

Replace `color:#007af4` → `color:#E95420` in:
1. `settings_page.cpp` line 28
2. `settings_page.ui` line 236
