# FR-10 Implementation Plan — Startup App Customization Enhancements

## Summary

Expand the Startup Apps page with features from QuentiumYT v1.5.0 and common competitor features. The work is scoped to **practical, cross-platform enhancements** that improve the daily usefulness of the startup manager without over-engineering niche features.

---

## Phase 1: Bug Fix — `getDesktopValue()` `=` Truncation (BUG-39)

**File:** `shared/nexis/utilities.h`

- [x] **1.1** Fix `getDesktopValue()` to use `section('=', 1)` instead of `split("=")` + `last()`, preserving values containing `=` signs (e.g. `Exec=env VAR=val /usr/bin/app`)

**Acceptance criteria:** `Exec=env QT_QPA_PLATFORM=xcb /usr/bin/app` correctly returns `env QT_QPA_PLATFORM=xcb /usr/bin/app`.

---

## Phase 2: Search/Filter Bar on Startup Apps List

**Files:** `startup_apps_page.ui`, `startup_apps_page.cpp`, `startup_apps_page.h`, `style.qss`

- [x] **2.1** Add a `QLineEdit` search bar (`txtSearchStartup`) to the page header row, between the title label and the "Add Startup App" button — matching the search pattern used on the Uninstaller and Services pages
- [x] **2.2** On `textChanged`, filter the QListWidget by hiding items whose app name doesn't contain the search text (case-insensitive)
- [x] **2.3** Style the search field consistently with `#txtSearchPackage` / `#txtSearchService` from the other pages
- [x] **2.4** Clear the search field when `loadApps()` is triggered (file system change or add/delete)

**Acceptance criteria:** Typing in the search bar instantly filters visible startup apps by name.

---

## Phase 3: Editor Dialog — GenericName and Icon Fields (Linux)

**Files:** `startup_app_edit.ui`, `startup_app_edit.h`, `linux/startup_app_edit.cpp`, `macos/startup_app_edit.cpp`

- [x] **3.1** Add `GENERIC_NAME_REG` (`^GenericName=.*`) and `ICON_REG` (`^Icon=.*`) regex defines to `startup_app_edit.h`
- [x] **3.2** Add `txtStartupAppGenericName` QLineEdit (placeholder: "Generic Name (e.g. Web Browser)") to the editor dialog UI at row 2
- [x] **3.3** Add `txtStartupAppIcon` QLineEdit (placeholder: "Icon name or path") to the editor dialog UI at row 4
- [x] **3.4** **Linux `startup_app_edit.cpp`**: Parse `GenericName=` and `Icon=` when editing; write them on save (both create and edit paths); include in the new-app template
- [x] **3.5** **macOS `startup_app_edit.cpp`**: Hide/disable the GenericName and Icon fields (not applicable to launchd plists) — use `#ifdef Q_OS_MACOS` or runtime platform check in `show()`
- [x] **3.6** Make GenericName and Icon optional (not required for validation) — only Name, Comment, and Command remain mandatory
- [x] **3.7** Resize the dialog minimum height to accommodate the two new rows (from 270 to ~330)

**Acceptance criteria:** Linux users can set GenericName and Icon; macOS users don't see inapplicable fields; new fields are optional.

---

## Phase 4: Display App Icon on the List Card

**Files:** `startup_app.h`, `startup_app.cpp`, `startup_apps_page.cpp`

- [x] **4.1** Add a `QString mIconName` member to `StartupApp` and an `iconName` parameter to the constructor
- [x] **4.2** In `startup_apps_page.cpp` (Linux path), parse `Icon=` from the `.desktop` file and pass it to the `StartupApp` constructor
- [x] **4.3** In the `StartupApp` constructor, attempt to resolve the icon:
  - First try `QIcon::fromTheme(iconName)` (works for standard freedesktop icon names like `firefox`, `org.gnome.Terminal`)
  - If null, try `QIcon(iconName)` (handles absolute file paths)
  - If still null, fall back to the existing generic `app.png`
- [x] **4.4** Set the resolved icon on `lblStartupAppIcon` via `setPixmap()` (scaled to the label's size constraints)
- [x] **4.5** macOS: Derive icon from the app bundle if the plist `ProgramArguments` points to a `.app` — use `QFileIconProvider` or `NSWorkspace` icon lookup; otherwise keep the generic icon

**Acceptance criteria:** Apps with an `Icon=` key (Linux) or `.app` bundle (macOS) show their real icon instead of the generic placeholder.

---

## Phase 5: Improved New-App Template (Linux)

**File:** `linux/nexis/Pages/StartupApps/startup_app_edit.cpp`

- [x] **5.1** Update the Linux new-app template to include the new optional fields and the `X-GNOME-Autostart-enabled=true` key:
  ```
  [Desktop Entry]
  Name=%1
  GenericName=%2
  Comment=%3
  Exec=%4
  Icon=%5
  Type=Application
  Terminal=false
  Hidden=false
  X-GNOME-Autostart-enabled=true
  ```
- [x] **5.2** Only emit `GenericName=`, `Icon=`, and `X-GNOME-Autostart-Delay=` lines if non-empty/non-zero (avoid blank keys)

**Acceptance criteria:** New `.desktop` files created by Nexis are more complete and compatible with GNOME session expectations.

---

## Phase 6: Documentation and Tracking

- [x] **6.1** Mark BUG-39 as `[x]` in `BUGS.md` with resolution note
- [x] **6.2** Mark FR-10 as `[x]` in `FEATURE_REQUESTS.md` with resolution note
- [x] **6.3** Mark all plan tasks `[x]` in this file

**Acceptance criteria:** All tracking files updated.

---

## Out of Scope

These features are technically possible but add complexity disproportionate to their value:

- **`OnlyShowIn`/`NotShowIn`** — Niche power-user feature; few desktop entries use it
- **System autostart (`/etc/xdg/autostart/`)** — Read-only display of system entries; adds confusion (users can't edit them)
- **Environment variable editor** — Complex table widget for a rare use case; env vars can be set in the `Exec` line directly
- **`Terminal=true` toggle** — Rarely used for autostart apps; would need macOS shell wrapper
- **macOS periodic execution** (`StartInterval`) — Different paradigm from "startup app"; better suited to a separate launchd manager feature

---

## Task Summary

| Phase | Tasks | Effort | Scope |
|-------|-------|--------|-------|
| 1. Bug fix (`getDesktopValue`) | 1 | Trivial | Shared |
| 2. Search/filter bar | 4 | Low | Shared (UI + page) |
| 3. GenericName + Icon fields | 7 | Medium | Shared UI + platform editors |
| 4. App icon display | 5 | Medium | Shared + platform |
| 5. Improved template | 2 | Low | Linux only |
| 6. Documentation | 3 | Trivial | Tracking files |
| **Total** | **22** | | |
