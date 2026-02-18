Perfect. Now I have all the information I need. Let me create the comprehensive research document:

---

# FR-01 Research: deb822 APT Source File Support

## Feature Summary

Add support for the modern `.sources` format (deb822 — RFC 822 key-value style) used by Debian 13+ and Ubuntu 24.04+. Currently, Nexis only parses the legacy one-line `.list` format from `/etc/apt/sources.list` and `/etc/apt/sources.list.d/*.list` files. The `.sources` format uses `/etc/apt/sources.list.d/*.sources` with a structured, multi-line key-value syntax that is easier to parse, extends more naturally, and better handles complex repository configurations.

**Upstream Reference:** QuentiumYT/Stacer commit `87279f6`

---

## Current Architecture

### File Structure & Parsing

**Linux implementation:** `/Users/luke/Documents/GitHub/Nexis/linux/nexis-core/Tools/apt_source_tool.cpp` (128 lines)

**Current locations scanned:**
- `/etc/apt/sources.list` (legacy monolithic file)
- `/etc/apt/sources.list.d/*.list` (legacy modular files)

**Regex pattern used (line 88):**
```cpp
QRegularExpression("^\\s{0,}#{0,}\\s{0,}deb")
```

This matches lines starting with optional whitespace, optional `#` comment marker, whitespace, and `deb` or `deb-src` keyword.

### Data Model

**APTSource struct** (`/Users/luke/Documents/GitHub/Nexis/shared/nexis-core/Tools/apt_source_tool.h`, lines 11-22):
```cpp
class APTSource {
public:
    QString filePath;      // Path to .list or .sources file
    bool isSource;         // true = deb-src, false = deb (binary)
    QString options;       // [arch=amd64 lang=en] etc.
    QString uri;           // http://archive.ubuntu.com/ubuntu
    QString distribution;  // xenial, focal, jammy, etc.
    QString components;    // main, universe, restricted, multiverse
    
    QString source;        // Full parsed line (for storage/display)
    bool isActive;         // false if line starts with #
};
```

### Parsing Logic (apt_source_tool.cpp:78-127)

1. **Discovery:** Lines 82-83 get all `.list` files from `/etc/apt/sources.list.d/` + the main `/etc/apt/sources.list`
2. **Per-file processing:** Lines 86-124 read each file, filter lines matching regex, split by whitespace
3. **Field extraction:** Lines 109-117
   - `sourceColumns[0]`: "deb" or "deb-src"
   - `sourceColumns[1]`: URI (after removing options)
   - `sourceColumns[2]`: Distribution
   - `sourceColumns[3+]`: Components (joined with spaces)
4. **Comment detection:** Lines 96, 98 — comment lines start with `#`
5. **Status tracking:** isActive = !line.startsWith('#')

### UI Flow

**APTSourceManagerPage** (`/Users/luke/Documents/GitHub/Nexis/shared/nexis/Pages/AptSourceManager/apt_source_manager_page.cpp`):

1. **Initialization (line 86):** `loadAptSources()` called on Linux
2. **Loading (lines 101-132):** Calls `ToolManager::ins()->getSourceList()` → `AptSourceTool::getSourceList()`
3. **Display (lines 108-119):** For each APTSource, creates `APTSourceRepositoryItem` widget with:
   - Checkbox for enable/disable
   - Source description label
   - Optional "(Source Code)" suffix for deb-src
4. **Editing (lines 380-390):** Launches `APTSourceEdit` dialog on double-click
5. **Modification:**
   - **Enable/Disable (apt_source_repository_item.cpp:62-64):** Toggles `#` prefix via `changeStatus()`
   - **Edit (apt_source_edit.cpp:53-73):** Reconstructs full line as `"deb [options] uri distribution components"`
   - **Delete (apt_source_manager_page.cpp:341-344):** Removes entire line via `removeAPTSource()`
   - **Add (apt_source_manager_page.cpp:231-251):** Calls `add-apt-repository` command

### Modification Methods (apt_source_tool.cpp:19-76)

**changeSource()** (lines 36-63) — generic write method:
- Reads entire file into `QStringList` (line 38)
- Linear search for matching line (lines 42-48)
- Replace or remove (lines 51-54)
- Write back with `sudo tee` (line 61)

**changeStatus()** (lines 65-76):
- Toggles comment prefix: `"# " + line` or remove `"#"`
- Calls `changeSource()` with modified line

**removeAPTSource()** (lines 19-22):
- Wrapper: calls `changeSource(aptSource, "")`

---

## The deb822 Format

### Structure

deb822 (RFC 822 key-value) format used in `/etc/apt/sources.list.d/*.sources` files:

```
Types: deb deb-src
URIs: http://archive.ubuntu.com/ubuntu/
Suites: focal focal-updates focal-backports
Components: main universe restricted multiverse
Signed-By: /etc/apt/keyrings/ubuntu-archive-keyring.gpg

Types: deb
URIs: http://security.ubuntu.com/ubuntu/
Suites: focal-security
Components: main universe restricted multiverse
```

### Key Differences vs `.list` Format

| Aspect | `.list` (old) | `.sources` (deb822/new) |
|--------|---------------|------------------------|
| **File format** | One entry per line | Stanza-based (RFC 822) |
| **Line syntax** | `deb [options] uri suite components` | Key-value pairs, multi-line |
| **Example** | `deb http://archive.ubuntu.com/ubuntu focal main` | `Types: deb`<br>`URIs: http://archive.ubuntu.com/ubuntu`<br>`Suites: focal`<br>`Components: main` |
| **Multiple URIs** | Repeat entire line per URI | Single `URIs:` field with space-separated values |
| **Multiple types** | Separate `deb` + `deb-src` lines | `Types: deb deb-src` in one stanza |
| **Options** | Inline: `[arch=amd64 lang=en]` | Separate field: `Architectures: amd64`<br>`Languages: en` |
| **Disabling** | Comment with `#` | `Enabled: no` field |
| **Signing key** | Inline in URI | Separate `Signed-By:` field with path |
| **File extension** | `.list` | `.sources` |
| **File location** | `/etc/apt/sources.list.d/` | `/etc/apt/sources.list.d/` (same dir) |

### Field Reference

**Core fields (required in most cases):**
- `Types:` — Space-separated list of `deb`, `deb-src` (or both)
- `URIs:` — Space-separated list of repository URLs
- `Suites:` — Space-separated distribution names (focal, jammy, etc.)
- `Components:` — Space-separated component names (main, universe, etc.)

**Optional fields:**
- `Enabled: yes|no` — Replaces comment-prefixing
- `Architectures:` — Space-separated architectures (amd64, i386, etc.)
- `Languages:` — Space-separated language codes
- `Targets:` — Package index types (Packages, Sources, Translations, etc.)
- `PDiffs:` — Enable/disable differential updates (yes/no)
- `By-Hash:` — Enable/disable hash-based indexing (yes/no)
- `Allow-Insecure: yes|no` — GPG signature bypass (dangerous)
- `Allow-Weak: yes|no` — Allow weak algorithms
- `Signed-By:` — Path to GPG keyring file (replaces inline key handling)
- `Comment:` — Free-text description

**Special stanza (metadata):**
```
X-Repolib-Name: My Repository
X-Repolib-Description: Custom description
```

---

## Code Areas Requiring Changes

### 1. **Parser Rewrite: apt_source_tool.cpp**

**Current line-based approach won't work for deb822.** Need:

- **New `.sources` file discovery** (line 82): Also scan for `*.sources` files in `/etc/apt/sources.list.d/`
- **New deb822 parser function:** Parse RFC 822 key-value stanzas (space-separated keys, values can span multiple lines if indented)
- **Normalization:** Convert both `.list` and `.sources` entries into the same `APTSource` struct

**Specific parsing challenges:**
1. **Multi-line values:** If a line is indented, it's a continuation of the previous field
   - Example:
     ```
     URIs: http://archive.ubuntu.com/ubuntu/
      http://mirror.example.com/ubuntu/
     ```
   - Both URLs should parse as a single space-separated list
2. **Multiple stanzas per file:** A `.sources` file can contain multiple repository entries, separated by blank lines
3. **Field name case-insensitivity:** RFC 822 keys are case-insensitive; standardize to Title-Case (e.g., `Types:`, `Suites:`)
4. **Comment handling:** deb822 uses `#` for comments too, but entire-line comments only (not inline)
5. **Enabled/disabled state:**
   - `.list`: Comment prefix `#`
   - `.sources`: `Enabled: no` field

**Proposed structure:**
```cpp
// New: Parse a single .sources stanza into APTSource
APTSourcePtr parseSourcesStanza(const QStringList &stanzaLines, const QString &filePath);

// New: Tokenize RFC 822 stanza (handle multi-line values)
QMap<QString, QString> parseRfc822Stanza(const QStringList &lines);

// Enhanced: AptSourceTool::getSourceList() calls both parsers
```

### 2. **File Writing: changeSource() Rewrite**

**Current approach (line-replacement via tee) won't work for multi-stanza deb822 files.**

- **For `.list` files:** Existing line-based logic can stay
- **For `.sources` files:** Need stanza-aware replacement
  - Find the stanza containing the target entry (match on URI + Suites + Components)
  - Replace/remove the entire stanza
  - Write back all stanzas

**Proposed function:**
```cpp
void changeSourcesStanza(const APTSourcePtr aptSource, const QString newStanzaText);
```

### 3. **Status Toggle: changeStatus() Enhancement**

**For `.list` files:** Existing comment-toggle logic stays

**For `.sources` files:**
```cpp
// Instead of prepending/removing #:
// Find "Enabled:" field, set to "no" or remove it (default = yes)
if (!status) {
    stanza["Enabled"] = "no";
} else {
    stanza.remove("Enabled");  // Default is enabled
}
```

### 4. **Reconstruction in apt_source_edit.cpp**

**Current reconstruction (line 59-64):**
```cpp
QString updatedAptSource = QString("%1 %2 %3 %4 %5")
    .arg(sourceType)
    .arg(ui->txtOptions->text())
    .arg(ui->txtUri->text())
    .arg(ui->txtDistribution->text())
    .arg(ui->txtComponents->text());
```

**For deb822, need conditional logic:**
```cpp
if (aptSource->filePath.endsWith(".sources")) {
    // Build deb822 stanza
    updatedAptSource = buildDeb822Stanza(sourceType, options, uri, distribution, components);
} else {
    // Build legacy .list line
    updatedAptSource = buildListLine(sourceType, options, uri, distribution, components);
}
```

### 5. **Parsing in apt_source_repository_item.cpp (line 43-51)**

The current code strips options with regex `\\s[\\[]+.*[\\]]+` because legacy format has inline options. deb822 has no inline options, so this regex won't apply.

```cpp
#ifdef Q_OS_LINUX
    if (mAptSource->filePath.endsWith(".sources")) {
        // deb822: no inline options, display differently
        ui->lblAptSourceName->setText(mAptSource->source);  // Full stanza description
    } else {
        // Legacy .list: strip inline options as before
        QString source = mAptSource->source;
        source.remove(QRegularExpression("\\s[\\[]+.*[\\]]+"));
        ui->lblAptSourceName->setText(source);
    }
#endif
```

---

## Key Implementation Gaps

### 1. No RFC 822 Parser in Codebase

Qt has no built-in RFC 822 parser. Need to write one:

```cpp
// Pseudo-code structure
class Rfc822Parser {
    static QMap<QString, QString> parse(const QStringList &lines) {
        // Handle line continuations (indented lines)
        // Split on ':' to get key-value pairs
        // Return normalized key-value map
    }
    
    static QStringList formatStanza(const QMap<QString, QString> &fields) {
        // Reverse: build RFC 822 stanza from map
    }
};
```

### 2. No Enabled/Disabled Field Handling

Current code assumes comments; deb822 uses `Enabled: yes/no` field. Need to:
- On read: Check `Enabled:` field (default = yes)
- On write: Add/remove `Enabled: no` instead of prepending `#`

### 3. Multi-Stanza File Handling

A single `.sources` file can have multiple stanzas (repositories). Current code assumes one entry per line. Need:
- Read entire file at once
- Split by blank lines to get stanzas
- Parse each stanza into separate `APTSource` objects
- On modification, reconstruct the entire file with updated stanza

### 4. No Validation of Reconstructed Lines

When user edits and saves via the dialog, the reconstructed line must:
- For `.list`: Maintain correct order (deb/deb-src, options, uri, distribution, components)
- For `.sources`: Maintain correct field names (Types:, URIs:, Suites:, Components:)
- Ensure no syntax errors before writing back

---

## Signal/Slot Connections Affected

**apt_source_manager_page.cpp:**
- `loadAptSources()` (line 101) — Will call enhanced `getSourceList()` that now handles both formats
- `on_btnEditAptSource_clicked()` (line 380) — Dialog will need awareness of file format
- `on_btnDeleteAptSource_clicked()` (line 305) — Will call `removeAPTSource()` which must handle both formats

**apt_source_edit.cpp:**
- `show()` (line 27) — Populate dialog fields from both `.list` and `.sources` format
- `on_btnSave_clicked()` (line 53) — Reconstruct appropriate format before calling `changeAPTSource()`

**apt_source_repository_item.cpp:**
- `on_checkAptSource_clicked()` (line 62) — Will call `changeAPTStatus()` which must handle both formats

---

## Platform Considerations

**This feature is Linux-only:**
- macOS uses Homebrew, not APT (apt_source_tool.cpp in macos/ is a stub)
- APT (`apt`, `add-apt-repository`) is Debian/Ubuntu exclusive
- deb822 format is Debian/Ubuntu exclusive (Debian 13+, Ubuntu 24.04+)

**Backward compatibility:**
- Systems with only `.list` files (Ubuntu 22.04 LTS and older) must continue working
- Parser must auto-detect file format and handle both
- Writing should preserve original format (don't convert `.list` to `.sources` automatically)

---

## Test Cases to Consider

1. **Mixed environment:** Some repos in `.list` files, others in `.sources` files
2. **Multi-stanza `.sources` file:** One file with multiple repository entries
3. **Multi-line RFC 822 fields:** URIs or other fields spanning multiple lines
4. **Disabled entries:** Both `# deb ...` (legacy) and `Enabled: no` (deb822)
5. **Complex options:** `[arch=amd64,i386 signed-by=/usr/share/...]` in legacy format
6. **Edge cases:**
   - Empty `.sources` file
   - `.sources` file with only comments
   - Malformed stanzas (missing required fields)

---

## File Paths Summary

| File | Role |
|------|------|
| `/Users/luke/Documents/GitHub/Nexis/linux/nexis-core/Tools/apt_source_tool.cpp` (128 lines) | **CORE PARSER** — Line-by-line `.list` parsing; must rewrite to handle deb822 |
| `/Users/luke/Documents/GitHub/Nexis/shared/nexis-core/Tools/apt_source_tool.h` | **DATA MODEL** — APTSource struct (compatible with both formats) |
| `/Users/luke/Documents/GitHub/Nexis/shared/nexis/Pages/AptSourceManager/apt_source_manager_page.cpp` (391 lines) | **UI CONTROLLER** — Calls getSourceList(), loadAptSources(); needs no changes |
| `/Users/luke/Documents/GitHub/Nexis/shared/nexis/Pages/AptSourceManager/apt_source_edit.cpp` (80 lines) | **EDIT DIALOG** — Reconstructs source line; needs format-aware logic (lines 59-64) |
| `/Users/luke/Documents/GitHub/Nexis/shared/nexis/Pages/AptSourceManager/apt_source_repository_item.cpp` (66 lines) | **ITEM WIDGET** — Displays sources; needs format-aware display logic (lines 43-51) |
| `/Users/luke/Documents/GitHub/Nexis/CMakeLists.txt` | **BUILD** — No changes needed; no new dependencies |
| `/Users/luke/Documents/GitHub/Nexis/shared/nexis/Managers/tool_manager.h` | **INTEGRATION** — Methods: getSourceList(), changeAPTSource(), changeAPTStatus(), removeAPTSource(), addAPTRepository() |

---

## Conclusion

The deb822 support requires a significant rewrite of the APT parsing layer to handle multi-stanza RFC 822 format while maintaining backward compatibility with legacy `.list` files. The main implementation burden is in `apt_source_tool.cpp` (parser rewrite, multi-stanza file handling, `Enabled:` field support). The UI layer requires only minor adjustments for format-aware reconstruction and display. The data model (`APTSource` struct) is already compatible with both formats.
