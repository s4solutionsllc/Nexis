Perfect! Now let me compile a comprehensive summary of my findings. Let me create the research file:

Based on my thorough investigation, I've identified all occurrences of the hardcoded Ubuntu font in the Nexis project. Here's the comprehensive report:

## FR-26 Research: Remove Hardcoded Ubuntu Font from .ui Files

### Executive Summary

The Nexis project currently hardcodes the "Ubuntu" font family in multiple locations:
- **5 .ui files** (Designer files) with explicit Ubuntu font properties
- **1 QSS stylesheet** (global style) with Ubuntu font-family fallback
- **1 C++ source file** with programmatic Ubuntu font setting
- **1 font resource** (Ubuntu-R.ttf) loaded in main.cpp via QFontDatabase
- **1 font file** bundled in resources (24 bytes in QRC file reference)

### 1. .UI Files with Ubuntu Font References

**File 1: `/Users/luke/Documents/GitHub/Nexis/shared/nexis/app.ui`**
- **Line 555-557**: pageTitle QLabel
  ```xml
  <property name="font">
    <font>
      <family>Ubuntu</family>
      <pointsize>12</pointsize>
    </font>
  </property>
  ```

**File 2: `/Users/luke/Documents/GitHub/Nexis/shared/nexis/Pages/Uninstaller/uninstallerpage.ui`**
- **Lines 121-124**: treeWidgetPackages (QTreeWidget)
  ```xml
  <property name="font">
    <font>
      <family>Ubuntu</family>
      <pointsize>10</pointsize>
    </font>
  </property>
  ```
- **Lines 242-246**: listWidgetSnapPackages (QListWidget)
  ```xml
  <property name="font">
    <font>
      <family>Ubuntu</family>
      <pointsize>10</pointsize>
    </font>
  </property>
  ```
- **Lines 331-335**: btnUninstall (QPushButton)
  ```xml
  <property name="font">
    <font>
      <family>Ubuntu</family>
    </font>
  </property>
  ```

**File 3: `/Users/luke/Documents/GitHub/Nexis/shared/nexis/Pages/Services/services_page.ui`**
- **Lines 51-55**: lblServicesTitle (QLabel)
  ```xml
  <property name="font">
    <font>
      <family>Ubuntu</family>
      <pointsize>11</pointsize>
    </font>
  </property>
  ```
- **Lines 130-135**: lblServiceStartupImg (QLabel)
  ```xml
  <property name="font">
    <font>
      <family>Ubuntu</family>
      <pointsize>10</pointsize>
    </font>
  </property>
  ```
- **Lines 183-188**: lblSystemRunningImg (QLabel)
  ```xml
  <property name="font">
    <font>
      <family>Ubuntu</family>
      <pointsize>10</pointsize>
    </font>
  </property>
  ```

**File 4: `/Users/luke/Documents/GitHub/Nexis/shared/nexis/Pages/StartupApps/startup_apps_page.ui`**
- **Lines 83-87**: btnAddStartupApp (QPushButton)
  ```xml
  <property name="font">
    <font>
      <family>Ubuntu</family>
    </font>
  </property>
  ```
- **Lines 107-113**: lblStartupAppsTitle (QLabel)
  ```xml
  <property name="font">
    <font>
      <family>Ubuntu</family>
      <pointsize>11</pointsize>
      <italic>false</italic>
    </font>
  </property>
  ```

**File 5: `/Users/luke/Documents/GitHub/Nexis/shared/nexis/Pages/AptSourceManager/apt_source_manager_page.ui`**
- **Lines 208-212**: btnEditAptSource (QPushButton)
  ```xml
  <property name="font">
    <font>
      <family>Ubuntu</family>
    </font>
  </property>
  ```
- **Lines 251-255**: btnDeleteAptSource (QPushButton)
  ```xml
  <property name="font">
    <font>
      <family>Ubuntu</family>
    </font>
  </property>
  ```
- **Lines 320-324**: btnAddAPTSourceRepository (QPushButton)
  ```xml
  <property name="font">
    <font>
      <family>Ubuntu</family>
    </font>
  </property>
  ```
- **Lines 353-357**: btnCancel (QPushButton)
  ```xml
  <property name="font">
    <font>
      <family>Ubuntu</family>
    </font>
  </property>
  ```
- **Lines 401-407**: lblAptSourceTitle (QLabel)
  ```xml
  <property name="font">
    <font>
      <family>Ubuntu</family>
      <pointsize>11</pointsize>
      <italic>false</italic>
    </font>
  </property>
  ```

**Total occurrences in .ui files: 15 separate font property blocks across 5 files**

### 2. QSS Stylesheet Font References

**File: `/Users/luke/Documents/GitHub/Nexis/shared/nexis/static/themes/default/style/style.qss`**
- **Line 346** (QMainWindow section):
  ```qss
  QMainWindow * {
      font-family: "Ubuntu", system-ui, sans-serif;
  }
  ```
  This is a **global selector** that applies to all widgets within QMainWindow. It acts as a fallback but is overridden by specific widget stylesheets.

### 3. Programmatic Font Setting in C++

**File: `/Users/luke/Documents/GitHub/Nexis/shared/nexis/Pages/AptSourceManager/apt_source_manager_page.cpp`**
- **Line 61**: Hard-coded Ubuntu font set on a dynamically created QTreeWidget
  ```cpp
  mTreeWidget->setFont(QFont("Ubuntu", 10));
  ```

### 4. Font Resource Loading

**File: `/Users/luke/Documents/GitHub/Nexis/shared/nexis/main.cpp`**
- **Line 121**: Loads Ubuntu font from bundled resource
  ```cpp
  QFontDatabase::addApplicationFont(":/static/font/Ubuntu-R.ttf");
  ```

### 5. Font Files in Project Resources

**QRC Entry: `/Users/luke/Documents/GitHub/Nexis/shared/nexis/static.qrc`**
- **Line 24**: Ubuntu font file bundled in resources
  ```xml
  <file>static/font/Ubuntu-R.ttf</file>
  ```

**Actual Font File: `/Users/luke/Documents/GitHub/Nexis/shared/nexis/static/font/Ubuntu-R.ttf`**
- This is the Ubuntu Regular variant (.ttf) file that Qt loads at startup

### 6. How Qt's Default Font System Works

When the Ubuntu font blocks are removed, Qt follows this fallback chain:

1. **Explicit Font Property**: First checks .ui/C++ font settings
2. **QSS Stylesheet**: Falls back to stylesheet font-family
3. **Application Font**: Falls back to QApplication::font()
4. **System Font**: Falls back to platform/desktop environment defaults
5. **Qt Default**: Uses hardcoded Qt default (usually "Helvetica" on most platforms)

Currently:
- The `QMainWindow * { font-family: "Ubuntu", system-ui, sans-serif; }` rule acts as a fallback
- Removing Ubuntu font blocks from .ui files will let this QSS rule take effect
- Removing Ubuntu from the QSS will allow the system font (system-ui fallback) to apply
- This is **exactly what we want** — using the system's native sans-serif font

### 7. Impact Analysis by Platform

**Ubuntu/Debian Linux:**
- Currently: Forces Ubuntu font (consistent branding)
- After removal: Falls back to system-ui → Noto Sans or equivalent system sans-serif
- Impact: Minimal visual change, uses system font for consistency with other apps

**Other Linux Distributions (Fedora, Arch, etc.):**
- Currently: Falls back to Ubuntu font approximation or substitution
- After removal: Uses native system sans-serif (Noto Sans, DejaVu Sans, Liberation Sans)
- Impact: Better visual integration with desktop environment

**macOS:**
- Currently: Falls back to system substitute (likely SF Pro Display or similar)
- After removal: Directly uses system UI font (SF Pro Display)
- Impact: Better visual consistency with native macOS apps

**Windows (if ever supported):**
- Currently: Falls back to Segoe UI or system substitute
- After removal: Uses native Windows sans-serif (Segoe UI)
- Impact: Better Windows integration

### 8. Should Ubuntu .ttf Be Removed?

**Yes, the Ubuntu-R.ttf file should also be removed because:**

1. It's only loaded in main.cpp and never explicitly referenced elsewhere
2. Removing it will slightly reduce application binary size
3. The system fonts provide better cross-platform consistency
4. No other project files depend on it
5. If the font is removed from .ui files and QSS, Qt will never use it anyway

**After font.ttf removal, update main.cpp:**
- Remove line 121: `QFontDatabase::addApplicationFont(":/static/font/Ubuntu-R.ttf");`
- Remove the file entry from static.qrc line 24

### 9. Summary of Changes Required

| Location | Type | Action | Count |
|----------|------|--------|-------|
| .ui files | font property | Remove `<family>Ubuntu</family>` blocks | 15 occurrences |
| style.qss | QSS rule | Change `"Ubuntu", system-ui, sans-serif` to `system-ui, sans-serif` | 1 line |
| apt_source_manager_page.cpp | C++ code | Change `QFont("Ubuntu", 10)` to `QFont()` or remove (use stylesheet) | 1 occurrence |
| main.cpp | Font registration | Remove `QFontDatabase::addApplicationFont(...)` call | 1 line |
| static.qrc | Resource entry | Remove Ubuntu-R.ttf file reference | 1 entry |
| static/font/ directory | File | Delete Ubuntu-R.ttf | 1 file |

### 10. Testing Expectations After Changes

After removing all Ubuntu font references:
- All text should render in the system's default sans-serif font
- Layout and spacing should remain identical (font metrics are similar)
- Cross-platform consistency will improve
- Binary size will decrease slightly
- App will respect user's system font preferences better
