# APT Repository Health & Info Dashboard — Design Spec

**Date:** 2026-03-20
**Feature:** Repo health indicators, descriptions, and guided repair for the APT Repository Manager (Linux) and Homebrew Packages (macOS) pages.
**Tracking:** TBD (will be assigned FR-XX after spec approval)

---

## Problem

The APT Repository Manager page shows repos as flat text lines with only an enable/disable toggle. Users have no visibility into:

- Whether a repo is healthy or broken (errors only surface during `apt update`)
- What a repo is for (especially repos added months ago or inherited from system setup)
- How to fix a broken repo (GPG errors, 404s, release mismatches)

## Solution Overview

Enrich repo cards with status badges and descriptions for at-a-glance scanning. Add a toggleable side detail panel with full diagnostics, explanations, and guided repair actions. Run periodic background health checks via DataRefreshService. Apply analogous features to the macOS Homebrew variant.

## Architecture

### New Components

| Component | Location | Purpose |
|-----------|----------|---------|
| `RepoHealthChecker` | `shared/nexis-core/Tools/` | Abstract base class for health check logic |
| `RepoHealthCheckerLinux` | `linux/nexis-core/Tools/` | Linux APT-specific health checks |
| `RepoHealthCheckerMac` | `macos/nexis-core/Tools/` | macOS Homebrew-specific health checks |
| `RepoHealthResult` | `shared/nexis-core/Tools/repo_health_types.h` | Data structs for health status, issues, and repair info |
| `RepoKnowledgeBase` | `shared/nexis-core/Tools/repo_knowledge_base.h/.cpp` | Static lookup table mapping URI patterns to names and descriptions |
| `RepoDetailPanel` | `shared/nexis/Pages/AptSourceManager/` | Shared base panel; platform content populated via `#ifdef Q_OS_MAC` blocks (see Platform Split Strategy) |

### Data Structs

```cpp
struct RepoHealthIssue {
    enum Severity { Info, Warning, Error };
    Severity severity;
    QString code;        // "gpg_expiring", "release_404", "suite_mismatch", etc.
    QString summary;     // "GPG key expires in 14 days"
    QString detail;      // Longer explanation for the detail panel
    QString repairCmd;   // Command to run, empty if no auto-repair
    QString repairLabel; // "Refresh signing key"
};

struct RepoHealthResult {
    enum Status { Unknown, Healthy, Warning, Error };
    Status status;                  // Worst severity across all issues
    QString name;                   // From knowledge base / Release / domain
    QString description;            // From knowledge base / Release
    QList<RepoHealthIssue> issues;  // All detected issues
    QDateTime lastChecked;
    QString releaseOrigin;          // Raw Origin field from Release file
};

struct RepoKnownInfo {
    QString name;        // "Deadsnakes PPA"
    QString description; // "Provides Python 3.7-3.13 not in default Ubuntu repos"
    QString url;         // Optional homepage URL
};
```

### APTSource Class Extensions

The existing `APTSource` struct needs two new fields to support health checks without re-parsing:

```cpp
class APTSource {
public:
    // ... existing fields ...
    enum Format { Legacy, Deb822 };
    Format format;          // Set by parser based on file extension (.list vs .sources)
    QString signedByPath;   // Extracted from options field (e.g., "/usr/share/keyrings/foo.gpg")
};
```

The shared parser in `apt_source_tool_shared.cpp` sets `format` based on which parsing path is used. `signedByPath` is extracted from the options string using `QRegularExpression("signed-by=([^\\],\\s]+)")` during `parseSourceListLine()`, or from the `Signed-By` key during `parseDeb822Stanza()`.

### Integration Points

- **DataRefreshService** — new `mRepoHealthRunning` guard (matching the `mDiskHealthRunning` pattern). New signal: `repoHealthChecked(QMap<QString, RepoHealthResult>)`. New public method: `triggerRepoHealthCheck()` for manual refresh. The page connects to `systemUpdatesChecked` and then calls `triggerRepoHealthCheck()` to chain health checks after update checks, keeping the service free of cross-concern coupling (the service does not internally chain update→health).
- **APTSourceManagerPage** — connects to the health signal, enriches card widgets, manages the side detail panel.
- **ToolManager** — wraps health checker instances (Linux/macOS) alongside existing tool instances.
- **CommandUtil** — executes repair commands (see Repair Error Handling section).

### Data Flow

1. Page receives `systemUpdatesChecked` signal and calls `DataRefreshService::triggerRepoHealthCheck()`
2. Platform-specific checker runs diagnostics per repo (network, GPG, suite, duplicates, format)
3. Knowledge base + Release file metadata provide repo name/description
4. Results stored in `QMap<QString, RepoHealthResult>` keyed by composite key (see Cache Key Definition)
5. Signal emitted on completion
6. Page reads cache, updates card badges and detail panel content
7. Cache cleared and re-checked when repos are added/removed/edited

### Cache Key Definition

**Linux:** Composite key `uri + " " + suites + " " + components` to uniquely identify each source entry. A single URI (e.g., `http://archive.ubuntu.com/ubuntu`) may appear with different suites, so URI alone is insufficient.

**macOS:** Package name (formula/cask name) as the key, since these are unique within Homebrew.

### Platform Split Strategy for RepoDetailPanel

`RepoDetailPanel` is a single shared widget with platform-conditional content sections:

- **Shared sections** (always present): Header (name + status badge), Description, Issues list with Repair buttons, Action bar
- **Linux-specific sections** (`#ifdef Q_OS_LINUX`): Metadata row shows File, Suite, Format, GPG status. Actions: Edit, Open URI, Disable.
- **macOS-specific sections** (`#ifdef Q_OS_MAC`): Metadata row shows Tap, Type (formula/cask), Version installed vs available. Actions: Update, Uninstall, Open Homepage.

This matches the existing pattern in `APTSourceManagerPage` which already uses `#ifdef` blocks to switch between APT and Homebrew UI modes.

## Health Checks

### Linux (APT)

| Check | Method | Severity |
|-------|--------|----------|
| Connection | HEAD request to `{uri}/dists/{suite}/InRelease` (5s timeout); skips non-HTTP schemes; HTTP 4xx = reachable | Error if network failure (DNS/timeout/refused) |
| Release file | Fetch `{uri}/dists/{suite}/InRelease` or `Release` | Error if 404 |
| GPG key | Use `signedByPath` from APTSource to check key file, or fall back to `apt-key list`; validate expiry | Warning <30 days, Error if expired/missing |
| Suite mismatch | Compare repo suite against `lsb_release -cs` | Warning if different codename |
| Duplicates | Compare normalized (uri + suite + components) across all sources | Warning on both entries |
| Deprecated format | Check `APTSource::format == Legacy` or missing `signedByPath` with `apt-key` usage | Warning (informational) |

### macOS (Homebrew)

Homebrew checks use batch commands rather than per-package invocations for performance:

| Check | Method | Severity |
|-------|--------|----------|
| Tap reachable | `brew tap-info --json` + check remote URL | Error if unreachable |
| Outdated packages | `brew outdated --json=v2` (single batch call, 30s timeout) | Warning with available version |
| Deprecated/disabled | `brew info --json=v2 --installed` (single batch call, 30s timeout) checking `deprecated`/`disabled` fields | Warning if deprecated, Error if disabled |
| Pinned packages | `brew list --pinned` | Info note |

### Performance

- Linux network requests: 5-second timeout per repo, sequential on background thread
- macOS batch commands: 30-second timeout per batch call (covers all packages at once)
- UI stays responsive — all checks run in `QtConcurrent::run()`
- Not persisted to disk — rebuilt each session (health status is transient)
- Manual "Refresh Health" button calls `DataRefreshService::triggerRepoHealthCheck()`, guarded by `mRepoHealthRunning` to prevent concurrent runs

## UI Design

### Enriched Cards

Cards grow from 45px to ~60px to accommodate a description line.

**Implementation:** Update `apt_source_repository_item.ui` minimum height to 60px. Layout restructuring: wrap `lblAptSourceName` and a new `lblDescription` in a `QVBoxLayout`, then insert that vertical layout in place of `lblAptSourceName` in the existing `QHBoxLayout`. Add programmatically in `APTSourceRepositoryItem::init()`:
- Left border: QSS `border-left: 3px solid <color>` on the card widget
- Status dot: 8px `QLabel` with round QSS stylesheet, inserted left of the new vertical layout
- Description line: `lblDescription` QLabel in the vertical layout below `lblAptSourceName`, gray color

**Health result data flow:** Add `void setHealthResult(const RepoHealthResult& result)` method to `APTSourceRepositoryItem`. The page calls this on each card after receiving the `repoHealthChecked` signal, and the card updates its status dot, border color, description, and inline issue text.

**Unknown status treatment:** Before the first health check completes, cards show `Unknown` status with a gray dot (using `@tertiaryText` token) and a question mark icon. Description falls back to knowledge base or domain name regardless of health status.

**Linux card:**
```
┌─[3px status border]──────────────────────────────────────────┐
│ ● deb http://archive.ubuntu.com/ubuntu jammy main        [✓] │
│   Ubuntu Main Repository — Core OS packages                   │
└───────────────────────────────────────────────────────────────┘
```

- **Left border** — 3px, colored by status
- **Status dot** — 8px circle, same color as border
- **Source line** — full `deb` line (row 1)
- **Description** — gray subtitle from knowledge base / Release / domain fallback (row 2)
- **Inline issue** — if warning/error, brief one-line summary in status color after description. Text elides with `...` at card edge (no wrapping to third line).
- **Toggle** — enable/disable checkbox unchanged, right side

**macOS card:** Same enrichment on QTreeWidget items. Status dot prepended, description as subtitle. Grouped sections remain.

**Theme colors:** Reuse existing tokens `@successColor`, `@warningColor`, and `@destructiveColor` from `values.ini` — these already provide correct green/amber/red values in both dark and light themes. No new tokens needed. (This avoids BUG-49 substring collision risks.)

**Accessibility:** Status dots include shape differentiation alongside color: checkmark icon (healthy), warning triangle (warning), X icon (error). Cards call `setAccessibleDescription()` with status text for screen readers.

Cards implement `refreshThemeColors()` per existing BUG-47 pattern.

### Side Detail Panel

**Behavior:**
- Hidden by default — repo list is full width
- Clicking a card toggles the panel in (QSplitter-based, ~40% width)
- Clicking the same card again or the close button (X) dismisses the panel
- Clicking a different card swaps content without close/reopen animation
- Panel width: ~40%, list: ~60%

**Panel layout (top to bottom):**

| Section | Content |
|---------|---------|
| Header | Repo name + status badge (pill) |
| Description | Full description text |
| Metadata | Platform-specific fields (see Platform Split Strategy) |
| Issues | List of issues, each with severity icon, summary, detail text, and Repair button (if repairable) |
| Actions | Platform-specific action buttons (see Platform Split Strategy) |

**Theme change handling:** The detail panel connects to `SignalMapper::sigChangedAppTheme` and re-renders severity icons and colored text when the theme changes while the panel is open.

### Repair Flow (Guided)

1. User clicks "Repair" on an issue in the detail panel
2. Confirmation dialog appears with:
   - Issue summary
   - Exact command that will be run
   - Explanation of what it does and why
3. User confirms → command runs (see Repair Error Handling)
4. On completion, re-checks that specific repo's health
5. Card badge and panel update inline with new status

### Repair Error Handling

Repair commands use `CommandUtil::execWithStatus()` (which returns `ExecResult` with exit code and output) rather than `sudoExec()` directly, wrapped with `pkexec` for privilege elevation on Linux.

| Scenario | Behavior |
|----------|----------|
| User cancels pkexec/elevation prompt | No-op, no error shown. Detail panel unchanged. |
| Command returns non-zero exit code | Show error in detail panel: "Repair failed" with command output in a scrollable text area. |
| Command times out (60s limit for repair operations) | Show timeout error in detail panel with suggestion to run manually. |
| polkit agent unavailable (headless/SSH) | Detection via `QProcess::errorOccurred`. Show message: "Privilege elevation unavailable. Run manually: `<command>`" |
| Command succeeds | Re-run health check on that specific repo. Update card and panel. Show brief success indicator. |

macOS repairs run brew commands directly (no sudo needed for most operations). `brew` errors are captured via stderr.

## Knowledge Base

Static `QMap<QString, RepoKnownInfo>` compiled into the binary. No external file I/O.

**i18n note:** Knowledge base description strings are wrapped in `QT_TR_NOOP()` for future translation support. v1 ships English-only, but the strings are extractable by `lupdate`.

**Built-in entries (~30-40 common repos), including:**

| Pattern | Name | Description |
|---------|------|-------------|
| `archive.ubuntu.com` | Ubuntu Main | Core OS packages and security updates |
| `security.ubuntu.com` | Ubuntu Security | Security patches for Ubuntu packages |
| `ppa.launchpadcontent.net/deadsnakes` | Deadsnakes PPA | Python 3.x alternate versions |
| `packages.microsoft.com/repos/vscode` | VS Code | Visual Studio Code editor |
| `packages.microsoft.com/repos/edge` | Microsoft Edge | Microsoft Edge browser |
| `dl.google.com/linux/chrome` | Google Chrome | Google Chrome browser |
| `deb.nodesource.com` | NodeSource | Node.js LTS and current releases |
| `download.docker.com` | Docker CE | Docker container engine |
| `apt.postgresql.org` | PostgreSQL | PostgreSQL database server |
| `repo.steampowered.com` | Steam | Valve Steam gaming platform |
| `packages.mozilla.org` | Mozilla APT | Firefox direct from Mozilla |

**Fallback chain:**
1. Match URI against knowledge base → name + description
2. No match → fetch `InRelease`/`Release`, parse `Origin` + `Description`
3. No Release metadata → domain extracted from URI

**macOS:** Uses `brew info --json` `desc` field directly. No knowledge base needed.

**Extensibility:** No user-editable entries in v1. The v2 approach would be a `QJsonDocument`-based override file in the config directory (`~/.config/nexis/repo-descriptions.json`), which is more Qt-idiomatic than a custom format.

## Testing Strategy

- **Unit tests** for `RepoHealthChecker` — mock network responses, verify correct issue detection for each check type
- **Unit tests** for `RepoKnowledgeBase` — verify pattern matching, fallback chain
- **Unit tests** for `RepoHealthResult` — verify status aggregation (worst severity wins)
- **Parser tests** — extend existing `test_apt_source_tool.cpp` for new `format` and `signedByPath` fields
- **Integration** — manual verification of UI on both platforms

## Scope Boundaries

**In scope:**
- Enriched cards with status and descriptions (Linux + macOS)
- Side detail panel with toggle behavior
- Background health checks via DataRefreshService
- Knowledge base for common repos
- Guided repair for detected issues
- `APTSource` struct extensions (`format`, `signedByPath`)
- Accessibility (shape + color status indicators, accessible descriptions)

**Out of scope (v1):**
- User-editable knowledge base entries
- Persistent health cache across sessions
- Health check notifications outside the page (e.g., system tray)
- Automatic repair without confirmation
- Package-level dependency analysis
- Non-English knowledge base translations (strings are marked for translation but not translated)
