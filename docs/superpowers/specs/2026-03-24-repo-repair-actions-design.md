# Repo Repair Actions — Design Spec

**Date:** 2026-03-24
**Feature:** FR-87 (extension) — Automated repair actions for APT Repository Health Dashboard
**Status:** Draft

---

## Problem

The health dashboard identifies 8 issue types on Linux and 4 on macOS, but only 2 issues (GPG expired/expiring) offer repair actions. Users see problems but can't fix them from within the app. The original intent of FR-87 was not just diagnosis but guided repair.

## Goals

1. Add automated repair actions for all fixable issue types
2. Help users research unreachable repositories via inline diagnostics
3. Provide a safe disable → remove workflow for broken repos
4. Replace the ad-hoc `repairCmd` string pattern with a typed action system

## Non-Goals

- Automated fix for suite mismatches (too risky — repo may not support the system codename)
- Removal of active (non-disabled) sources
- Running the entire app as root

---

## Design

### 1. Data Model

#### `RepoRepairAction` (new struct in `repo_health_types.h`)

```cpp
struct RepoRepairAction {
    enum Type {
        RunCommand,         // Simple shell command (GPG refresh, brew upgrade)
        ConvertToDeb822,    // Download key + write .sources + disable .list
        RemoveDuplicate,    // Comment out duplicate entry in source file
        DiagnoseConnection, // Run diagnostic checks, expand results inline
        DisableSource,      // Comment out (.list) or Enabled: no (.sources)
        EnableSource,       // Uncomment (.list) or remove Enabled: no (.sources)
        RemoveSource        // Delete entry/file — only for already-disabled sources
    };
    Type type;
    QString label;          // Button text
    QString command;        // For RunCommand type only
    QVariantMap context;    // Extra data for the engine (file path, key URL, etc.)
};
```

#### Changes to `RepoHealthIssue`

Replace `repairCmd` and `repairLabel` fields with:

```cpp
QList<RepoRepairAction> actions;
```

An issue can offer multiple actions (e.g., "Diagnose" + "Disable").

### 2. RepoRepairEngine

Abstract base class in `shared/nexis-core/Tools/` with platform-specific subclasses (`RepoRepairEngineLinux`, `RepoRepairEngineMac`), following the same pattern as `RepoHealthChecker`. The engine is **stateless** — each method takes its inputs and returns a result, with no persistent state. This avoids thread-safety concerns since `diagnoseConnection` runs async.

File-modifying operations use temp file + `pkexec` to write to `/etc/apt/` without running the app as root. Before any file modification, the engine copies the original file to a backup at `~/.local/share/nexis/backups/<filename>.<timestamp>` so changes can be recovered if something goes wrong.

```cpp
// Abstract base
class RepoRepairEngine : public QObject {
    Q_OBJECT
public:
    struct RepairResult {
        bool success;
        QString message;       // User-facing summary
        QString errorDetail;   // Technical detail on failure
    };

    // Shared (non-virtual)
    RepairResult runCommand(const QString &command);

    // Platform-specific (pure virtual)
    virtual RepairResult convertToDeb822(const APTSourcePtr &source) = 0;
    virtual RepairResult removeDuplicate(const APTSourcePtr &source) = 0;
    virtual RepairResult disableSource(const APTSourcePtr &source) = 0;
    virtual RepairResult enableSource(const APTSourcePtr &source) = 0;
    virtual RepairResult removeSource(const APTSourcePtr &source) = 0;
    virtual void diagnoseConnection(const APTSourcePtr &source) = 0;

signals:
    void diagnoseFinished(const DiagnoseResult &result);
};
```

Platform split (mirrors `RepoHealthChecker` pattern):
- `shared/nexis-core/Tools/repo_repair_engine.h` — abstract base, `RepairResult`, `DiagnoseResult`/`DiagnoseStep` structs
- `shared/nexis-core/Tools/repo_repair_engine.cpp` — shared non-virtual logic (`runCommand`, backup helper)
- `linux/nexis-core/Tools/repo_repair_engine_linux.h` — `RepoRepairEngineLinux` declaration
- `linux/nexis-core/Tools/repo_repair_engine.cpp` — Linux implementations (convert, duplicate, disable, enable, remove, diagnose)
- `macos/nexis-core/Tools/repo_repair_engine_macos.h` — `RepoRepairEngineMac` declaration
- `macos/nexis-core/Tools/repo_repair_engine.cpp` — macOS stubs (all return no-op; brew uses `RunCommand`)

`ToolManager` instantiates the platform-specific subclass, same as it does for `RepoHealthChecker`.

**Privilege escalation abstraction:** File writes go through a `writeFileElevated(tempPath, destPath)` helper that calls `pkexec cp`. Unit tests override this via a `MockFileWriter` test helper that writes directly, allowing deb822 generation and file manipulation logic to be tested without `pkexec`.

### 3. Diagnose Connection Flow

#### Data structures

```cpp
struct DiagnoseStep {
    enum Status { Ok, Warning, Failed };
    QString check;    // "DNS Resolution", "Base Domain", etc.
    Status status = Ok;
    QString detail;   // "Domain resolves to 185.199.108.153"
};

struct DiagnoseResult {
    QList<DiagnoseStep> steps;
    QString suggestion;                    // Summary recommendation
    QList<RepoRepairAction> followUpActions; // Typed actions (Open in Browser, Search Online, Disable)
};
```

Follow-up actions use the existing `RepoRepairAction` type. "Open in Browser" and "Search Online" are `RunCommand` actions with `xdg-open`/`open` commands. "Disable" is a `DisableSource` action. This keeps all action dispatch going through the same typed pipeline.

#### Diagnostic steps (in order)

1. **DNS Resolution** — `QDnsLookup` on the domain. Distinguishes NXDOMAIN from DNS server unreachable.
2. **Base Domain** — HTTP HEAD to `https://<domain>/`. If succeeds but full URI fails → repo restructured.
3. **Protocol Check** — try alternate protocol (HTTP ↔ HTTPS).
4. **Redirect Check** — HTTP GET with redirect following. Report new location if redirected.
5. **Knowledge Base Lookup** — compare current URI against canonical URL for known repos.

#### Suggestion synthesis

| Result | Suggestion |
|--------|-----------|
| DNS failed | "This domain no longer exists. The repository may have been discontinued." |
| Base domain alive, path 404 | "The server is up but the repository path has changed." |
| Redirect found | "This repository has moved to `<new-url>`. Consider updating your source." |
| Knowledge base mismatch | "The canonical URL for this repo is `<url>`. Your source may be outdated." |
| Protocol mismatch | "This repository is available over `<protocol>` instead." |

#### UX

- "Diagnose" button disabled and replaced with spinner while checks run (async via `QtConcurrent::run`). A second click while in-flight is ignored.
- Results render inline in the issue card: compact list of steps (status icon + check name + detail)
- Suggestion text below the steps
- Follow-up action buttons rendered from `DiagnoseResult::followUpActions` (e.g., "Open in Browser", "Search Online", "Disable")

### 4. Convert Legacy to Deb822

#### Steps

1. **Resolve GPG key** (chain):
   - `signed-by` already set → use it, skip download
   - Knowledge base has `keyUrl` for this repo → download to `/usr/share/keyrings/<domain>-archive-keyring.gpg`
   - Auto-discover: try `<uri>/gpg.key`, `<uri>/key.gpg`, `<uri>/signing-key.asc`, `<uri>/gpg`, `<uri>/Release.key`, `<uri>/apt-key.gpg` — use first 200 response
   - All fail → proceed without `signed-by`, include warning in result
   - Validate downloaded content: must start with `-----BEGIN PGP` or be binary keyring

2. **Generate deb822 content:**
   ```
   Types: deb
   URIs: <uri>
   Suites: <suites>
   Components: <components>
   Signed-By: /usr/share/keyrings/<domain>-archive-keyring.gpg
   ```

3. **Write `.sources` file** — temp file → `pkexec cp` to `/etc/apt/sources.list.d/<name>.sources`. Filename derived from domain (e.g., `pkg-cloudflare-com.sources`). If filename exists, append numeric suffix: `pkg-cloudflare-com-1.sources`, `pkg-cloudflare-com-2.sources`, etc. (max 10 retries before failing).

4. **Comment out old `.list` entry** — `pkexec sed` to prepend `# Converted to deb822 by Nexis\n#` to the matching line. If file has multiple entries, only comment the specific line.

5. **Return result** — success message or failure with which step failed.

#### Knowledge base extension

Add `keyUrl` field to `RepoKnownInfo`:

```cpp
struct RepoKnownInfo {
    QString name;
    QString description;
    QString url;
    QString keyUrl;     // GPG key download URL (new)
};
```

Populate for known repos (Google, Docker, Microsoft, GitHub CLI, NodeSource, PostgreSQL, etc.).

### 5. Remove Duplicate

1. Health checker identifies duplicates and records the file path and source details in the action `context`
2. Read the source file
3. Comment out the duplicate line: `# Disabled by Nexis: duplicate entry\n#<original line>`
4. Write back via temp + `pkexec`
5. Re-check health

If both duplicates are in the same file, comment out the second occurrence only.

### 6. Disable / Enable / Remove Source

#### Disable

- **Legacy `.list`** — prepend `#` to the line
- **Deb822 `.sources`** — add `Enabled: no` to the stanza
- After disabling: card shows repo as inactive, "Disable" button becomes "Enable", "Remove" button appears

#### Enable (separate action type: `EnableSource`)

- **Legacy** — remove `#` prefix
- **Deb822** — remove `Enabled: no` line
- Explicit `EnableSource` type and `enableSource()` method — no toggling. The action type always matches what the button label says, avoiding state-race bugs if the file is modified externally between render and click.

#### Remove (disabled entries only)

1. Engine verifies `isActive == false` — refuses to remove active sources
2. Confirmation dialog: **"This will permanently delete this repository entry. This action cannot be undone."**
3. **Legacy** — remove the commented line. If only entry in file, delete file.
4. **Deb822** — remove the stanza. If only stanza in file, delete file.
5. Via `pkexec`
6. Reload source list

### 7. Issue → Action Mapping

#### Linux

| Issue Code | Actions |
|------------|---------|
| `connection_error` | `DiagnoseConnection`, `DisableSource` |
| `release_404` | `DiagnoseConnection`, `DisableSource` |
| `legacy_format` | `ConvertToDeb822` |
| `no_signed_by` | `ConvertToDeb822` (see dedup note below) |
| `duplicate_source` | `RemoveDuplicate` |
| `gpg_expired` | `RunCommand` (GPG refresh — see GPG note below) |
| `gpg_expiring` | `RunCommand` (GPG refresh — see GPG note below) |
| `gpg_missing` | `ConvertToDeb822` (re-downloads key) |
| `suite_mismatch` | None (informational only) |
| Disabled source (any) | `EnableSource`, `RemoveSource` |

**Dedup note:** `no_signed_by` only fires when `format == Legacy`, so a source will have both `legacy_format` and `no_signed_by` issues. Since `ConvertToDeb822` resolves both (conversion adds `signed-by`), the `no_signed_by` issue should suppress its action button when a `ConvertToDeb822` action already exists on another issue for the same source. The health checker handles this by checking whether a `ConvertToDeb822` action was already added to the result's issue list before adding another.

**GPG note:** The existing GPG refresh command (`gpg --recv-keys` without a key fingerprint) is incomplete. The implementation should extract the key fingerprint from the keyring first via `gpg --list-keys --with-colons`, then pass it to `--recv-keys`. If no fingerprint can be extracted, the action is not offered.

#### macOS

| Issue Code | Actions |
|------------|---------|
| `outdated` | `RunCommand` (`brew upgrade <name>`) |
| `deprecated` | None (informational only) |
| `disabled` | `RunCommand` (`brew uninstall <name>`) |
| `tap_unreachable` | None (informational only) |
| `pinned` | None (informational only) |

No changes to macOS repair logic beyond migrating from `repairCmd`/`repairLabel` to `actions`.

### 8. Detail Panel UI Changes

- `addIssueWidget()` renders a horizontal row of buttons from `issue.actions` (instead of single repair button)
- Buttons are compact (fixed height 26px, left-aligned)
- "Remove" button uses object name `repoRemoveBtn` with a QSS rule: `#repoRemoveBtn { color: @destructiveColor; }` — no inline hardcoded colors
- Diagnose results expand inline below the issue summary with step-by-step output
- Diagnose step list uses object name `repoDiagnoseStep` for QSS styling
- Spinner replaces "Diagnose" button while async checks run; button is disabled until completion

### 9. Page Integration

- Replace `onRepairRequested(command, label)` signal/slot with `onRepairActionRequested(RepoRepairAction, APTSourcePtr)`
- Handler switches on `action.type`:
  - `RunCommand` → pkexec as before
  - `DiagnoseConnection` → async, connect to `diagnoseFinished`
  - All others → call engine method, show result, re-trigger health check + reload sources
- Confirmation dialog before all file-modifying actions (except Diagnose)
- "Remove" confirmation includes the extra "cannot be undone" warning

---

## Files

### New Files

| File | Purpose |
|------|---------|
| `shared/nexis-core/Tools/repo_repair_engine.h` | Abstract base class, RepairResult, DiagnoseResult, DiagnoseStep |
| `shared/nexis-core/Tools/repo_repair_engine.cpp` | Shared non-virtual logic (runCommand, backup helper, writeFileElevated) |
| `linux/nexis-core/Tools/repo_repair_engine_linux.h` | `RepoRepairEngineLinux` declaration |
| `linux/nexis-core/Tools/repo_repair_engine.cpp` | Linux implementations (convert, duplicate, disable, enable, remove, diagnose) |
| `macos/nexis-core/Tools/repo_repair_engine_macos.h` | `RepoRepairEngineMac` declaration |
| `macos/nexis-core/Tools/repo_repair_engine.cpp` | macOS stubs |
| `tests/core/test_repo_repair_engine.cpp` | Unit tests for repair logic (uses MockFileWriter) |

### Modified Files

| File | Changes |
|------|---------|
| `shared/nexis-core/Tools/repo_health_types.h` | Add `RepoRepairAction`, replace `repairCmd`/`repairLabel` with `actions` list |
| `shared/nexis-core/Tools/repo_knowledge_base.h` | Add `keyUrl` to `RepoKnownInfo` |
| `shared/nexis-core/Tools/repo_knowledge_base.cpp` | Populate `keyUrl` for known repos |
| `linux/nexis-core/Tools/repo_health_checker.cpp` | Populate `actions` instead of `repairCmd`/`repairLabel` |
| `macos/nexis-core/Tools/repo_health_checker.cpp` | Populate `actions` instead of `repairCmd`/`repairLabel` |
| `shared/nexis/Pages/AptSourceManager/repo_detail_panel.h` | New signals for action dispatch, diagnose result display |
| `shared/nexis/Pages/AptSourceManager/repo_detail_panel.cpp` | Render action button rows, inline diagnose expansion |
| `shared/nexis/Pages/AptSourceManager/apt_source_manager_page.h` | New slot, engine member |
| `shared/nexis/Pages/AptSourceManager/apt_source_manager_page.cpp` | Typed action dispatch handler |
| `shared/nexis/Managers/tool_manager.h/.cpp` | Accessor for `RepoRepairEngine` |
| `CMakeLists.txt` | Add new source files |
| `tests/CMakeLists.txt` | Register new test target |

---

## Testing Strategy

- **Unit tests** for `RepoRepairEngineLinux`: uses `MockFileWriter` to test deb822 generation, duplicate commenting, disable/enable logic, and remove logic without `pkexec`. Verifies file content output, backup creation, and edge cases (multi-entry files, only-entry-in-file deletion).
- **Unit tests** for diagnose flow: mock network responses (QNetworkAccessManager override or test HTTP server), verify step classification and suggestion synthesis.
- **Unit tests** for `ConvertToDeb822`: verify correct deb822 output for various input formats, GPG key resolution chain, key content validation.
- **Existing tests** must continue passing after `repairCmd` → `actions` migration. The `RepoHealthCheckerTests` and `RepoKnowledgeBaseTests` must still pass.
- **Knowledge base tests**: verify `keyUrl` populated for known repos, verify auto-discover URL list.
- **`pkexec` integration** cannot be unit-tested. The `writeFileElevated` helper is abstracted behind an interface so tests bypass it. Manual testing on a real system covers the elevated write path.
- **Manual integration**: test on a system with real repos — convert a legacy source, disable/enable a source, remove a disabled source, diagnose an unreachable repo.
