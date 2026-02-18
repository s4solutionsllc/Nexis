Perfect. Now I have enough information to compile a comprehensive research report. Let me create the report.

## Comprehensive Research Report: FR-18 System Cleaner Exclusion Rules

Based on my thorough investigation of the Nexis codebase, here is a detailed analysis of how exclusion rules could be integrated into the System Cleaner:

---

### 1. SYSTEM CLEANER SCAN/DISPLAY/CLEAN WORKFLOW

#### **Entry Point: SystemCleanerPage Class**
**File:** `/Users/luke/Documents/GitHub/Nexis/shared/nexis/Pages/SystemCleaner/system_cleaner_page.h` (lines 14-113)  
**File:** `/Users/luke/Documents/GitHub/Nexis/shared/nexis/Pages/SystemCleaner/system_cleaner_page.cpp` (lines 1-543)

The System Cleaner operates in three distinct phases:

##### **Phase 1: Category Selection (stackedWidget page 0)**
- User is shown 6 cleanup categories with checkboxes:
  1. Package Caches (Homebrew, APT, DNF, Pacman)
  2. Crash Reports
  3. Application Logs
  4. Application Caches
  5. Trash
  6. Dev Tool Caches
- Each category is an independent boolean checkbox (lines 396-401)
- All checkboxes must be unchecked by default (lines 262-267, 509)

##### **Phase 2: Scan (systemScan() worker thread)**
**Lines:** `178-198`

Scan is launched asynchronously via `QtConcurrent::run()` (line 437) to avoid UI blocking:

```cpp
void SystemCleanerPage::systemScan()
{
    // Worker thread: only I/O, no UI access
    if (mScanPackageCache)
        mPackageCaches = tmr->getPackageCaches();
    if (mScanCrashReports)
        mCrashReports = im->getCrashReports();
    if (mScanAppLog)
        mAppLogs = im->getAppLogs();
    if (mScanAppCache)
        mAppCaches = im->getAppCaches();
    if (mScanDevToolCache)
        mDevToolCaches = im->getDevToolCaches();
    
    emit scanFinishedS();
}
```

**Data sources for each category:**

| Category | Source Method | File Path |
|----------|---------------|-----------|
| Package Caches | `ToolManager::getPackageCaches()` | Linux: `/var/cache/apt`, `/var/cache/dnf`, `/var/cache/pacman/pkg`; macOS: Homebrew cache |
| Crash Reports | `InfoManager::getCrashReports()` | macOS: `~/Library/Logs/DiagnosticReports`, `/Library/Logs/DiagnosticReports`; Linux: `/var/crash` |
| App Logs | `InfoManager::getAppLogs()` | macOS: `~/Library/Logs`, `/var/log`; Linux: `/var/log` |
| App Caches | `InfoManager::getAppCaches()` | macOS: `~/Library/Caches`; Linux: `~/.cache` |
| Dev Tool Caches | `InfoManager::getDevToolCaches()` | Electron: `~/.config/*/Cache`, `~/.config/*/GPUCache`; npm: `~/.npm`, gradle: `~/.gradle/caches`, cargo: `~/.cargo/registry`, etc. |
| Trash | Hard-coded paths | macOS: `~/.Trash`; Linux: `~/.local/share/Trash` |

##### **Phase 3: Results Display (onScanFinished() main thread)**
**Lines:** `200-277`

Results are populated into a `QTreeWidget` (treeWidgetScanResult) with hierarchical structure:

1. **Root items** = Categories (line 111): `QTreeWidgetItem *root = new QTreeWidgetItem(ui->treeWidgetScanResult);`
2. **Child items** = Individual files (lines 148-158): Custom `ByteTreeWidget` subclass

**Tree structure example:**
```
[✓] Package Caches (15)
├─ [✓] file1.tar.gz (245 MB)
├─ [✓] file2.deb (120 MB)
└─ [✓] file3.pkg (85 MB)
[✓] App Logs (42)
├─ [✓] system.log (5 MB)
└─ [✓] debug.log (2 MB)
```

##### **Phase 4: File Selection (UI Interaction)**
**Lines:** `166-176` (on_treeWidgetScanResult_itemClicked)  
**Lines:** `522-532` (on_checkSelectAll_clicked)

- Users click checkboxes to mark files for deletion (line 168-170)
- Parent-child cascading: If parent is checked, all children get checked (lines 173-174)
- "Select All" button propagates state to all categories and files (lines 524-531)
- Sorting options available (lines 534-542): by name A-Z, name Z-A, size small-large, size large-small

##### **Phase 5: Clean (systemClean() worker thread)**
**Lines:** `296-349`

Clean is launched asynchronously via `QtConcurrent::run()` (line 488):

1. **File collection** (main thread, lines 455-483):
   - Iterate through tree widget
   - Build `mFilesToDelete` list (QStringList) from checked items (line 468-469)
   - Record parent/child indices in `mChildrenToRemove` list (line 470)
   - Special handling for Trash: set `mCleanTrash = true` (line 475)

2. **Deletion** (worker thread, lines 296-349):
   - For each file in `mFilesToDelete`:
     - If directory: empty contents but preserve directory (lines 332-339)
     - If file: delete directly (line 337)
   - For Trash: platform-specific deletion (lines 302-317)
   - Sum total size deleted (line 322)

3. **UI update** (main thread, lines 351-388):
   - Remove deleted items from tree widget (lines 357-364)
   - Recalculate root item sizes (lines 368-377)
   - Display cleaned size message (line 379-380)
   - Re-enable buttons for next scan (lines 382-385)

---

### 2. TREE WIDGET STRUCTURE & DATA STORAGE

#### **QTreeWidget Layout (system_cleaner_page.ui, lines 735-780)**

```xml
<widget class="QTreeWidget" name="treeWidgetScanResult">
    <property name="columnCount"><number>2</number></property>
    <column><property name="text"><string notr="true">1</string></property></column>
</widget>
```

**Two columns:**
1. Column 0: File/directory name + checkbox
2. Column 1: Size (formatted bytes)

#### **Custom Data Storage (ByteTreeWidget)**
**File:** `/Users/luke/Documents/GitHub/Nexis/shared/nexis/Pages/SystemCleaner/byte_tree_widget.h` (lines 1-20)  
**File:** `/Users/luke/Documents/GitHub/Nexis/shared/nexis/Pages/SystemCleaner/byte_tree_widget.cpp` (lines 1-22)

```cpp
void ByteTreeWidget::setValues(const QString &text, const quint64 &size, const QVariant &data) {
    this->setText(0, text);                          // Display name
    this->setText(1, FormatUtil::formatBytes(size)); // Display size
    this->setData(1, SortRole, size);                // Sort by bytes
    this->setData(2, 0, data);                       // Store file path or category
    this->setCheckState(0, Qt::Unchecked);           // Unchecked by default
}
```

**Data roles used:**
- `data(2, 0)`: File path (QString) for children; Category enum (CleanCategories) for roots
- `data(2, 1)`: Category title (QString, roots only)
- `data(3, 0)`: Parent directory path (QString, roots only)
- `data(1, SortRole)`: Byte size for custom sorting (quint64)
- `checkState(0)`: Qt::Checked / Qt::Unchecked for selection

---

### 3. DATA FLOW FROM SCANNER TO UI

```
InfoManager / ToolManager (platform-specific implementations)
         ↓
SystemInfo::getCrashReports() [line 117-130, macos/nexis-core/Info/system_info.cpp]
SystemInfo::getAppLogs()      [line 132-146, macos/nexis-core/Info/system_info.cpp]
SystemInfo::getAppCaches()    [line 148-165, macos/nexis-core/Info/system_info.cpp]
SystemInfo::getDevToolCaches()[line 167-207, macos/nexis-core/Info/system_info.cpp]
ToolManager::getPackageCaches()[line 76-86, macos/nexis/Managers/tool_manager.cpp]
         ↓
QFileInfoList (returned to system_cleaner_page.cpp)
         ↓
systemScan() [line 178-198] - Worker thread collects files
         ↓
onScanFinished() [line 200-277] - Main thread populates tree
         ↓
addTreeRoot() [line 109-146] - Creates category root items
         ↓
addTreeChild() [line 148-158] - Creates child file items
         ↓
QTreeWidget display (stackedWidget page 1)
```

---

### 4. FILE SELECTION MECHANISM

#### **Checkbox State Tracking**
- Every tree item has `checkState(0)` property (Qt::Checked / Qt::Unchecked)
- Initialized to `Qt::Unchecked` in `ByteTreeWidget::setValues()` (line 9, byte_tree_widget.cpp)
- User clicks toggle via `on_treeWidgetScanResult_itemClicked()` (line 166-176)
- Cascading: Parent check state propagates to all children (lines 173-174)

#### **Validation Before Clean**
```cpp
bool SystemCleanerPage::cleanValid()
{
    for (int i = 0; i < ui->treeWidgetScanResult->topLevelItemCount(); ++i) {
        QTreeWidgetItem *it = ui->treeWidgetScanResult->topLevelItem(i);
        if (it->checkState(0) == Qt::Checked)
            return true;
        for (int j = 0; j < it->childCount(); ++j)
            if (it->child(j)->checkState(0) == Qt::Checked)
                return true;
    }
    return false;
}
```

At least one file must be checked, or "Clean" button is disabled.

---

### 5. PROPOSED EXCLUSION POINTS IN PIPELINE

#### **Option 1: Filter During Scan (Recommended)**
**Location:** `systemScan()` [line 178-198]  
**Advantage:** Cleaner results, reduced memory footprint  
**Disadvantage:** Need to apply exclusions before populating tree

Implementation approach:
```cpp
void SystemCleanerPage::systemScan() {
    if (mScanAppCaches) {
        QFileInfoList allCaches = im->getAppCaches();
        mAppCaches = filterByExclusions(allCaches, mAppCacheExclusions);
    }
    // ... similarly for other categories
}
```

#### **Option 2: Filter During Results Display**
**Location:** `onScanFinished()` [line 200-277]  
**Advantage:** All data collected first, can preview exclusions  
**Disadvantage:** Extra filtering on main thread

Implementation approach:
```cpp
void SystemCleanerPage::onScanFinished() {
    if (mScanAppCache) {
        mAppCaches = applyExclusions(mAppCaches);
        totalSize += addTreeRoot(APPLICATION_CACHES, ...);
    }
}
```

#### **Option 3: Filter During Clean**
**Location:** `systemClean()` [line 296-349]  
**Advantage:** Allows last-minute exclusions  
**Disadvantage:** Most complex, requires re-checking at deletion time

Implementation approach:
```cpp
void SystemCleanerPage::systemClean() {
    QStringList filteredList;
    for (const QString &file : mFilesToDelete) {
        if (!shouldExclude(file))
            filteredList << file;
    }
    // Delete from filteredList instead of mFilesToDelete
}
```

**RECOMMENDATION:** Option 1 (filter during scan) is cleanest - integrates at the source, reduces memory, simplifies deletion logic.

---

### 6. EXCLUSION RULE TYPES & MATCHING STRATEGIES

#### **6.1 Path-Based Exclusions**

**Exact path matching:**
```
/Users/luke/Library/Caches/specific_app
/var/log/system.log
```

**Wildcard patterns:**
```
/home/*/.cache/*/tmp.*
/var/log/*.gz
```

**Regex patterns:**
```
.*\.log\.[0-9]+$    (files like access.log.1, access.log.2)
^/var/cache/.*old$  (cache dirs ending in "old")
```

#### **6.2 Filename-Based Exclusions**

**Exact filename:**
```
important.log
config.ini
```

**Glob patterns:**
```
*.lock
.git*
```

**Regex:**
```
^\..*          (hidden files)
.*\.(log|tmp)$ (by extension)
```

#### **6.3 Directory-Based Exclusions (Preserve Categories)**

**Category-level:** Exclude entire Package Caches category
**Directory-level:** Skip specific app cache directories (`~/.cache/Signal`, `~/.cache/Firefox`)
**Recursive:** Skip directory and all contents (`~/.cache/com.google.Chrome/**`)

#### **6.4 Age-Based Exclusions (Future Enhancement)**

```
files older than 30 days
files modified before 2024-01-01
```

---

### 7. SETTINGS PERSISTENCE

#### **SettingManager Architecture**
**File:** `/Users/luke/Documents/GitHub/Nexis/shared/nexis/Managers/setting_manager.h` (lines 1-101)  
**File:** `/Users/luke/Documents/GitHub/Nexis/shared/nexis/Managers/setting_manager.cpp` (lines 1-80+)

Settings are stored in `~/.config/Nexis/nexis/settings.ini` (line 6 of setting_manager.cpp):
```cpp
mSettings = new QSettings(QString("%1/settings.ini").arg(mConfigPath), QSettings::IniFormat);
```

**Pattern for adding new settings:**

1. **Define setting key** in `SettingKeys` namespace:
```cpp
// In setting_manager.h
namespace SettingKeys {
    const QString SystemCleanerExclusions("SystemCleanerExclusions");
}
```

2. **Add getter/setter methods** in SettingManager:
```cpp
// In setting_manager.h
void setSystemCleanerExclusions(const QStringList &rules);
QStringList getSystemCleanerExclusions() const;

// In setting_manager.cpp
void SettingManager::setSystemCleanerExclusions(const QStringList &rules) {
    mSettings->setValue(SettingKeys::SystemCleanerExclusions, rules);
}

QStringList SettingManager::getSystemCleanerExclusions() const {
    return mSettings->value(SettingKeys::SystemCleanerExclusions, QStringList()).toStringList();
}
```

3. **Persist JSON for complex data:**
```cpp
// Store list of rule objects as JSON
QJsonArray rulesArray;
for (const ExclusionRule &rule : mExclusions) {
    rulesArray.append(rule.toJson());
}
mSettings->setValue(SettingKeys::SystemCleanerExclusions, 
                    QString::fromUtf8(QJsonDocument(rulesArray).toJson()));
```

---

### 8. UI DESIGN CONSIDERATIONS

#### **8.1 Integration Points**

**Option A: Inline in System Cleaner Page (Lightweight)**
- Add "Settings" button below category checkboxes on page 0
- Opens modal dialog with exclusion rule editor
- Avoids navigation to separate Settings page

**Option B: Global Settings Page (Comprehensive)**
- Add "System Cleaner" tab to existing Settings page
- Alongside Theme, Language, Alert Percentages
- Persists globally across all sessions

**Option C: Scan Results Page (Context-Aware)**
- Add "Exclude" button in results tree widget
- Right-click context menu on files/folders
- Dynamic exclusion without re-scanning

#### **8.2 Exclusion Rule Editor UI Elements**

**Required components:**

1. **Rule type selector (QComboBox):**
   - Exact path
   - Path with wildcards
   - Filename (glob)
   - Filename (regex)
   - Category (package caches / app caches / etc.)

2. **Input field (QLineEdit + validation):**
   - Placeholder text: "Enter exclusion pattern"
   - Real-time regex validation feedback
   - Red border if invalid regex

3. **Category filter (QCheckBox group):**
   ```
   ☑ Apply to Package Caches
   ☑ Apply to Crash Reports
   ☑ Apply to App Logs
   ☑ Apply to App Caches
   ☑ Apply to Dev Tool Caches
   ☑ Apply to Trash
   ```

4. **Rule list (QListWidget):**
   - Each rule shows type, pattern, categories
   - Delete buttons per rule
   - Import/Export buttons

5. **Preview (QLabel or QPlainTextEdit):**
   - Show example matches/non-matches
   - "This pattern will exclude: /var/log/*.gz"

#### **8.3 Settings Dialog Layout**

```
┌─────────────────────────────────────────┐
│ System Cleaner Exclusion Rules          │
├─────────────────────────────────────────┤
│                                         │
│ Exclusion Rules:                        │
│ ┌──────────────────────────────────────┐│
│ │ [!] *.lock (applies to App Caches)   │║
│ │ [!] ^\..*  (applies to all)          │║
│ │ [!] /var/log/old* (applies to Logs)  │║
│ └──────────────────────────────────────┘│
│  [+ Add Rule]  [- Delete Selected]      │
│                                         │
│ New Rule:                               │
│ Type: [Glob pattern ▼]                  │
│ Pattern: [________________]              │
│ Apply to: ☑️ Caches ☑️ Logs  ☐ Trash   │
│ [+ Add]  [Cancel]                       │
│                                         │
│ [Import from file]  [Export to file]    │
│                      [OK] [Cancel]      │
└─────────────────────────────────────────┘
```

---

### 9. EXCLUSION RULE STORAGE & DATA STRUCTURE

#### **JSON Format (Recommended for complexity)**
```json
{
  "rules": [
    {
      "id": "rule-001",
      "name": "Ignore lock files",
      "type": "glob",
      "pattern": "*.lock",
      "categories": ["APPLICATION_CACHES"],
      "enabled": true,
      "createdAt": "2026-02-18T10:30:00Z"
    },
    {
      "id": "rule-002",
      "name": "Hide Firefox cache",
      "type": "path",
      "pattern": "/home/*/.cache/firefox/**",
      "categories": ["APPLICATION_CACHES", "DEV_TOOL_CACHES"],
      "enabled": true,
      "createdAt": "2026-02-18T10:32:00Z"
    },
    {
      "id": "rule-003",
      "name": "Skip hidden files",
      "type": "regex",
      "pattern": "^\\.[^\\/]*$",
      "categories": ["APPLICATION_CACHES", "APPLICATION_LOGS"],
      "enabled": false,
      "createdAt": "2026-02-18T10:35:00Z"
    }
  ]
}
```

#### **QSettings INI Format (Simple)**
```ini
[SystemCleaner/Exclusions]
rules=*.lock|^\..*|/var/log/old*
ruleTypes=glob|regex|path
categories_0=APPLICATION_CACHES
categories_1=APPLICATION_CACHES,APPLICATION_LOGS
categories_2=APPLICATION_LOGS
```

**Better approach:** Use JSON stored as string in QSettings:
```cpp
// In setting_manager.cpp
void SettingManager::setSystemCleanerExclusions(const QString &jsonRules) {
    mSettings->setValue("SystemCleaner/ExclusionRules", jsonRules);
}
```

---

### 10. IMPLEMENTATION ARCHITECTURE

#### **New Classes Required**

**ExclusionRule (header in nexis/Pages/SystemCleaner/)**
```cpp
class ExclusionRule {
    enum RuleType { ExactPath, WildcardPath, GlobPattern, RegexPattern };
    
    QString id;
    QString name;
    RuleType type;
    QString pattern;
    QSet<int> categories;  // Set of CleanCategories enums
    bool enabled;
    QString createdAt;
    
    bool matches(const QString &filePath) const;
    bool matchesCategory(CleanCategories cat) const;
    QJsonObject toJson() const;
    static ExclusionRule fromJson(const QJsonObject &obj);
};
```

**ExclusionRuleManager (new manager in nexis/Managers/)**
```cpp
class ExclusionRuleManager {
    static ExclusionRuleManager *ins();
    
    void loadRules();
    void saveRules();
    void addRule(const ExclusionRule &rule);
    void deleteRule(const QString &ruleId);
    void updateRule(const ExclusionRule &rule);
    
    bool shouldExclude(const QString &filePath, CleanCategories cat);
    QList<ExclusionRule> getRules() const;
};
```

**ExclusionRuleDialog (new dialog in nexis/Pages/SystemCleaner/)**
```cpp
class ExclusionRuleDialog : public QDialog {
    void setupUI();
    void onAddRuleClicked();
    void onDeleteRuleClicked();
    void onPatternChanged(const QString &pattern);
    void validateRegex(const QString &pattern);
    
    ExclusionRule buildRule();
};
```

#### **Integration Points**

1. **In SystemCleanerPage constructor:**
   ```cpp
   mExclusionManager = ExclusionRuleManager::ins();
   mExclusionManager->loadRules();
   ```

2. **In systemScan():**
   ```cpp
   if (mScanAppCache) {
       QFileInfoList allCaches = im->getAppCaches();
       mAppCaches = filterExcluded(allCaches, APPLICATION_CACHES);
   }
   ```

3. **Add filter helper method:**
   ```cpp
   private:
       QFileInfoList filterExcluded(const QFileInfoList &files, 
                                    CleanCategories cat);
   ```

---

### 11. WORKFLOW WITH EXCLUSION RULES

```
User launches cleaner
        ↓
Category selection screen (page 0)
        ↓
[User clicks "Configure Exclusions..." button]
        ↓
ExclusionRuleDialog opens
        ↓
User adds/edits/deletes rules
        ↓
Dialog saves to SettingManager
        ↓
[User clicks Scan]
        ↓
systemScan() starts (worker thread)
        ↓
For each category:
    ├─ Get all files (InfoManager/ToolManager)
    ├─ Filter by exclusion rules (ExclusionRuleManager)
    └─ Store filtered list in mAppCaches, mPackageCaches, etc.
        ↓
onScanFinished() populates tree (main thread)
        ↓
Results display WITHOUT excluded items
        ↓
User checks items
        ↓
[User clicks Clean]
        ↓
systemClean() deletes checked items (worker thread)
        ↓
Success - deleted items removed from tree
```

---

### 12. KEY FILES & SUMMARY TABLE

| File Path | Lines | Purpose |
|-----------|-------|---------|
| `system_cleaner_page.h` | 1-113 | Main page class definition, state vars, slots |
| `system_cleaner_page.cpp` | 1-543 | Core scan/clean logic, tree population |
| `system_cleaner_page.ui` | 1-851 | UI layout, stacked widget, tree widget |
| `byte_tree_widget.h` | 1-20 | Custom tree item class for byte sorting |
| `byte_tree_widget.cpp` | 1-22 | Size data storage, custom comparator |
| `system_info.h` | 1-41 | Interface for platform-specific methods |
| `macos/system_info.cpp` | 117-207 | macOS implementations of getters |
| `linux/system_info.cpp` | 99-170 | Linux implementations of getters |
| `tool_manager.h` | 1-45 | ToolManager interface |
| `macos/tool_manager.cpp` | 76-86 | macOS getPackageCaches (Homebrew) |
| `linux/tool_manager.cpp` | 85-100 | Linux getPackageCaches (APT/YUM/Pacman) |
| `setting_manager.h` | 1-101 | SettingManager interface with keys |
| `setting_manager.cpp` | 1-80+ | Settings persistence in INI format |
| `nexis_roles.h` | 1-14 | Custom data roles (SortRole, etc.) |

---

### 13. THREADING & SYNCHRONIZATION NOTES

**Critical observations:**

1. **Scan runs on worker thread** (line 437: `QtConcurrent::run([this]() { systemScan(); })`)
   - Only I/O operations allowed
   - No UI access
   - Results stored in member variables (mPackageCaches, etc.)

2. **Signal emitted on finish** (line 197: `emit scanFinishedS()`)
   - Connected on main thread (line 105)
   - Triggers onScanFinished() on main thread

3. **Thread safety flags** (lines 106-107):
   - `mScanInProgress = true` prevents overlapping operations
   - `mCleanInProgress = true` prevents concurrent clean

4. **Exclusion filtering should be thread-safe:**
   - If filtering during scan: ExclusionRuleManager read from worker thread
   - If filtering during display: ExclusionRuleManager read from main thread
   - **Recommendation:** Load rules once in main thread before scan starts

---

### 14. EDGE CASES & SPECIAL HANDLING

1. **Trash is special** (lines 243-250, 465-481):
   - No child items in tree (noChild=true, line 246)
   - Handled separately in clean logic
   - Exclusions for Trash: either skip entire trash or allow it

2. **Dev Tool Caches post-processing** (lines 223-241):
   - Electron app cache dirs renamed from "Cache"→"appName/Cache"
   - Exclusions must match both original and renamed paths

3. **Empty directories preserved** (lines 330-339):
   - Directories emptied but not deleted (preserve OS expectations)
   - Exclusions can skip entire directories or individual files within them

4. **Size calculation** (lines 321-323):
   - Totals calculated before deletion
   - If item excluded, its size not counted for cleaned total

---

### 15. TESTING CONSIDERATIONS

**Test scenarios for exclusion rules:**

1. Exact path exclusion prevents that specific file from appearing
2. Glob pattern `*.lock` excludes all .lock files across all categories
3. Regex `^\..*` excludes hidden files (starting with .)
4. Category filter: rule applies only to selected categories
5. Disabled rule: toggling enabled/disabled applies immediately on re-scan
6. Multiple rules: file matching ANY enabled rule is excluded
7. Rule deletion: previously excluded items reappear on re-scan
8. Settings persistence: rules survive app restart

---

This comprehensive report provides everything needed to implement FR-18: exclusion rules for the System Cleaner, including architecture, UI design, data persistence, and integration points within the existing codebase.
