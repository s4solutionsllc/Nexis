# FR-01 Research: deb822 APT Source File Support

## Feature Summary

Add support for the modern `.sources` format (deb822 — RFC 822 key-value style) used by Debian 13+ and Ubuntu 24.04+. Currently, Nexis only parses the legacy one-line `.list` format from `/etc/apt/sources.list` and `/etc/apt/sources.list.d/*.list` files. The `.sources` format uses `/etc/apt/sources.list.d/*.sources` with a structured, multi-line key-value syntax.

**Upstream Reference:** QuentiumYT/Stacer commit `87279f6`

---

## Current Architecture

### Data Model

**APTSource struct** (`shared/nexis-core/Tools/apt_source_tool.h`, lines 11-22):
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
    bool isActive;         // false if line starts with # (legacy) or Enabled: no (deb822)
};
typedef QSharedPointer<APTSource> APTSourcePtr;
```

### ToolManager Interface

`shared/nexis/Managers/tool_manager.h` (lines 36-41):
```cpp
bool checkSourceRepository() const;
QList<APTSourcePtr> getSourceList() const;
void removeAPTSource(const APTSourcePtr source);
void changeAPTStatus(const APTSourcePtr aptSource, const bool status);
void changeAPTSource(const APTSourcePtr aptSource, const QString newSource);
void addAPTRepository(const QString &repository, const bool isSource);
```

Note: `changeAPTSource` currently takes a `QString` — this must change to `APTSourcePtr` for format-aware writing.

### Linux Parser Implementation

**File:** `linux/nexis-core/Tools/apt_source_tool.cpp` (128 lines)

Constants:
```cpp
static constexpr const char *APT_SOURCES_LIST_D_PATH = "/etc/apt/sources.list.d";
static constexpr const char *APT_SOURCES_LIST_PATH   = "/etc/apt/sources.list";
```

**`getSourceList()`** (lines 78-127):
1. Discovery: `QDir(APT_SOURCES_LIST_D_PATH).entryInfoList({"*.list"})` + `QFileInfo(APT_SOURCES_LIST_PATH)`
2. Per-file: `FileUtil::readListFromFile()` → filter with regex `"^\\s{0,}#{0,}\\s{0,}deb"`
3. Per-line: Split by whitespace, extract uri (col 1), distribution (col 2), components (col 3+)
4. Status: `isActive = !line.startsWith('#')`

**`changeSource()`** (lines 36-63):
- Reads file into QStringList (one line per element)
- Linear search for matching line via `indexOf(aptSource->source)`
- Replace or remove that line
- Write back via `CommandUtil::sudoExec("tee", {filePath}, data)`

**`changeStatus()`** (lines 65-76):
- Toggles comment prefix: `"# " + line` or removes `"#"`
- Delegates to `changeSource()`

**`removeAPTSource()`** (lines 19-22):
- Calls `changeSource(aptSource, "")` — empty string triggers line removal

**`addRepository()`** (lines 24-34):
- Runs `CommandUtil::sudoExec("add-apt-repository", {"-y", repo})` — external tool handles file creation

### UI Layer

**APTSourceManagerPage** (`shared/nexis/Pages/AptSourceManager/apt_source_manager_page.cpp`, 391 lines):
- `loadAptSources()`: Calls `ToolManager::ins()->getSourceList()`, creates `APTSourceRepositoryItem` per source
- `on_btnEditAptSource_clicked()`: Launches `APTSourceEdit` dialog
- `on_btnDeleteAptSource_clicked()`: Calls `ToolManager::ins()->removeAPTSource()`
- `on_btnAddAPTSourceRepository_clicked()`: Calls `ToolManager::ins()->addAPTRepository()`
- Search filtering via `QListWidgetItem::setData(5, searchData)`

**APTSourceEdit** (`shared/nexis/Pages/AptSourceManager/apt_source_edit.cpp`, 80 lines):
- `show()`: Populates fields from `selectedAptSource->isSource`, `options`, `uri`, `distribution`, `components`
- `on_btnSave_clicked()`: Rebuilds `.list` format string: `"deb [options] uri distribution components"`
  Calls `ToolManager::ins()->changeAPTSource(selectedAptSource, updatedString)`

**APTSourceRepositoryItem** (`shared/nexis/Pages/AptSourceManager/apt_source_repository_item.cpp`, 66 lines):
- `init()`: Strips inline options with regex `\\s[\\[]+.*[\\]]+`, sets label text
- `on_checkAptSource_clicked()`: Calls `ToolManager::ins()->changeAPTStatus()`

### macOS Implementation (Homebrew Adapter)

`macos/nexis-core/Tools/apt_source_tool.cpp` (90 lines) — Maps APT interface to Homebrew packages. Not relevant for deb822.

---

## The deb822 Format Specification

### Structure

Stanza-based RFC 822 key-value format. Multiple stanzas per file, separated by blank lines.

```
Types: deb deb-src
URIs: https://deb.debian.org/debian
Suites: trixie trixie-updates
Components: main contrib non-free non-free-firmware
Signed-By: /usr/share/keyrings/debian-archive-keyring.gpg

Types: deb deb-src
URIs: https://deb.debian.org/debian-security
Suites: trixie-security
Components: main contrib non-free non-free-firmware
Signed-By: /usr/share/keyrings/debian-archive-keyring.gpg
```

### Key Differences from `.list` Format

| Aspect | `.list` (legacy) | `.sources` (deb822) |
|--------|-------------------|----------------------|
| Structure | Single line per repo | Multi-line stanza per repo |
| Multiple types | Separate line for `deb` and `deb-src` | `Types: deb deb-src` on one field |
| Multiple suites | Separate line per suite | `Suites: trixie trixie-updates` on one field |
| Options | Square brackets `[arch=amd64,armel]` | Separate fields: `Architectures: amd64 armel` |
| Comments | `#` anywhere on line | `#` only at start of line |
| Enable/Disable | Comment/uncomment with `#` | `Enabled: yes/no` field |
| GPG keys | Inline `[signed-by=/path]` | `Signed-By:` field (supports embedded keys) |
| File extension | `.list` | `.sources` |
| Deprecation | Deprecated, removal after ~2029 | Current recommended format |

### Required Fields

| Field | Description |
|-------|-------------|
| `Types` | `deb` (binary), `deb-src` (source), or both. Space-separated. |
| `URIs` | Base URL(s). Multiple URIs space-separated. |
| `Suites` | Distribution codenames. Multiple suites space-separated. |
| `Components` | Repository sections. Required unless Suites is an exact path ending in `/`. |

### Optional Fields

| Field | Values | Description |
|-------|--------|-------------|
| `Enabled` | `yes` / `no` (default: `yes`) | Enable/disable without deleting |
| `Signed-By` | File path or embedded PGP key | GPG keyring. Supports inline PGP blocks (APT 2.3.10+). |
| `Architectures` | Space-separated | Limits architectures |
| `Languages` | Space-separated | Translation languages |
| `Targets` | Space-separated | Download targets |
| `PDiffs` | `yes` / `no` | Partial index updates |
| `By-Hash` | `yes` / `no` / `force` | Hash-based URI construction |
| `Trusted` | `yes` / `no` | Override trust assessment (dangerous) |
| `Allow-Insecure` | `yes` / `no` | Allow unsigned repos |
| `Allow-Weak` | `yes` / `no` | Allow weak crypto |
| `Check-Valid-Until` | `yes` / `no` | Replay attack detection |

### Multiline Values (Embedded GPG Keys)

Continuation lines indented with exactly one leading space. Empty lines represented as ` .` (space+dot):
```
Signed-By:
 -----BEGIN PGP PUBLIC KEY BLOCK-----
 .
 mDMEYCQjIxYJKwYBBAHaRw8BAQdAD/P5Nvvnvk66SxBBHDb...
 -----END PGP PUBLIC KEY BLOCK-----
```

### Default Distro Files

- **Debian 13:** `/etc/apt/sources.list.d/debian.sources`
- **Ubuntu 24.04:** `/etc/apt/sources.list.d/ubuntu.sources`

### Migration Tool

APT provides `apt modernize-sources` to convert `.list` → `.sources`.

---

## QuentiumYT Implementation (Commit `87279f6`)

### Key Design Decisions

1. **Renamed `distribution` → `suites`** in APTSource struct (matches APT terminology)
2. **Changed `changeSource()` signature** from `QString newSource` to `APTSourcePtr newSource` — structured object instead of raw string
3. **`removeAPTSource()`** passes `nullptr` instead of empty string to `changeSource()`
4. **`changeStatus()`** creates copy of APTSource with `isActive` flag set, delegates to `changeSource()` — format-specific enable/disable logic lives in `changeSource()`
5. **Edit dialog** builds `APTSourcePtr` object instead of format-specific string
6. **Stanza matching** uses synthetic source string: `"deb uri suites components"` compared against `aptSource->source`
7. **Preserves field ordering** when rewriting stanzas (iterates original line order, inserts new fields at end)
8. **Preserves `Signed-By` multi-line values** with special continuation-line handling
9. **Preserves comments** within stanzas at their original line positions
10. **Deletes file** when all stanzas are removed (instead of leaving empty file)
11. **Also deletes `.list` file** when last line is removed

### UX Improvements in Same Commit

- Selection cleared after reload, delete, edit save
- "Adding..." button feedback during `add-apt-repository` execution
- Search filter preserved across add/delete operations
- Placeholder changed to `'ppa:deadsnakes/ppa'` format
- Translation updates: "Distribution" → "Suites"

---

## Implementation Impact Analysis

### Files Requiring Changes

| File | Change Scope | Description |
|------|-------------|-------------|
| `shared/nexis-core/Tools/apt_source_tool.h` | **Moderate** | Rename `distribution` → `suites`, change `changeSource()` signature |
| `linux/nexis-core/Tools/apt_source_tool.cpp` | **Major** | Full parser rewrite: deb822 parsing, stanza-aware writing, `Enabled:` field |
| `shared/nexis/Managers/tool_manager.h` | **Minor** | Update `changeAPTSource()` signature |
| `linux/nexis/Managers/tool_manager.cpp` | **Minor** | Forward new signature |
| `shared/nexis/Pages/AptSourceManager/apt_source_edit.cpp` | **Moderate** | Build `APTSourcePtr` instead of format string |
| `shared/nexis/Pages/AptSourceManager/apt_source_edit.ui` | **Minor** | Rename `txtDistribution` → `txtSuites` |
| `shared/nexis/Pages/AptSourceManager/apt_source_manager_page.cpp` | **Minor** | UX improvements, selection clearing |
| `shared/nexis/Pages/AptSourceManager/apt_source_repository_item.cpp` | **None** | Display logic already works (source field is a synthetic string) |
| Translation `.ts` files | **Minor** | "Distribution" → "Suites" terminology |

### Files NOT Requiring Changes

- `macos/nexis-core/Tools/apt_source_tool.cpp` — Homebrew adapter, no APT
- `macos/nexis/Managers/tool_manager.cpp` — macOS ToolManager, no APT changes needed (just forward new signature)
- `CMakeLists.txt` — No new files or dependencies
- `shared/nexis/Pages/AptSourceManager/apt_source_manager_page.ui` — Layout unchanged
- `shared/nexis/Pages/AptSourceManager/apt_source_repository_item.ui` — Layout unchanged

### Data Flow Impact

1. **Read path:** `getSourceList()` must discover `*.sources` files AND `*.list` files, parse each format appropriately, produce the same `APTSourcePtr` objects
2. **Write path:** `changeSource()` branches on `filePath.endsWith(".sources")` vs `.list`
3. **Status toggle:** `changeStatus()` creates modified copy, delegates to `changeSource()`
4. **Delete:** `removeAPTSource()` passes nullptr, `changeSource()` handles stanza removal or line removal
5. **UI display:** `APTSourceRepositoryItem` already works — `source` field is populated uniformly for both formats

---

## Platform Considerations

- **Linux-only feature:** macOS uses Homebrew, not APT
- **Backward compatibility:** Systems with only `.list` files must continue working (Ubuntu 22.04 LTS and older)
- **Format preservation:** Writing should preserve original format (don't convert `.list` to `.sources`)
- **Mixed environments:** Both `.list` and `.sources` files can coexist in `/etc/apt/sources.list.d/`

---

## Edge Cases

1. Mixed `.list` and `.sources` files in same directory
2. Multiple stanzas in single `.sources` file
3. Multi-line RFC 822 fields (embedded GPG keys in `Signed-By:`)
4. Disabled sources: both `# deb ...` (legacy) and `Enabled: no` (deb822)
5. `Types: deb deb-src` — single stanza represents both binary and source (should create one APTSource entry, not two)
6. Empty `.sources` file or comment-only files
7. Malformed stanzas (missing required fields)
8. File deletion when last stanza/line is removed
9. Continuation lines in values (indented with space)
10. Unknown fields (should be preserved on rewrite)
