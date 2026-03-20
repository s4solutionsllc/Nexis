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
| `RepoDetailPanel` | `shared/nexis/Pages/AptSourceManager/` | Side panel widget showing full repo detail and repair actions |

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

### Integration Points

- **DataRefreshService** — new periodic health check phase runs after the existing update check on the hourly timer. New signal: `repoHealthChecked(QMap<QString, RepoHealthResult>)`.
- **APTSourceManagerPage** — connects to the health signal, enriches card widgets, manages the side detail panel.
- **ToolManager** — wraps health checker instances (Linux/macOS) alongside existing tool instances.
- **CommandUtil** — executes repair commands with sudo elevation (Linux) or direct brew commands (macOS).

### Data Flow

1. DataRefreshService triggers health check in `QtConcurrent::run()`
2. Platform-specific checker runs diagnostics per repo (network, GPG, suite, duplicates, format)
3. Knowledge base + Release file metadata provide repo name/description
4. Results stored in `QMap<QString, RepoHealthResult>` keyed by normalized URI (Linux) or package name (macOS)
5. Signal emitted on completion
6. Page reads cache, updates card badges and detail panel content
7. Cache cleared and re-checked when repos are added/removed/edited

## Health Checks

### Linux (APT)

| Check | Method | Severity |
|-------|--------|----------|
| Connection | HEAD request to repo URI (5s timeout) | Error if unreachable |
| Release file | Fetch `{uri}/dists/{suite}/InRelease` or `Release` | Error if 404 |
| GPG key | Parse `signed-by` or check `apt-key list`; validate expiry | Warning <30 days, Error if expired/missing |
| Suite mismatch | Compare repo suite against `lsb_release -cs` | Warning if different codename |
| Duplicates | Compare normalized (uri + suite + components) across all sources | Warning on both entries |
| Deprecated format | Check for `apt-key` usage instead of `signed-by`; legacy `sources.list` | Warning (informational) |

### macOS (Homebrew)

| Check | Method | Severity |
|-------|--------|----------|
| Tap reachable | `brew tap-info --json` + check remote URL | Error if unreachable |
| Outdated packages | `brew outdated --json` | Warning with available version |
| Deprecated/disabled | `brew info --json` checking `deprecated`/`disabled` fields | Warning if deprecated, Error if disabled |
| Pinned packages | `brew list --pinned` | Info note |

### Performance

- Network requests: 5-second timeout per repo
- Checks run sequentially per repo on background thread (UI stays responsive)
- Not persisted to disk — rebuilt each session (health status is transient)
- Manual "Refresh Health" button triggers immediate out-of-cycle check

## UI Design

### Enriched Cards

Cards grow from 45px to ~60px to accommodate a description line.

**Linux card:**
```
┌─[3px status border]──────────────────────────────────────────┐
│ ● deb http://archive.ubuntu.com/ubuntu jammy main        [✓] │
│   Ubuntu Main Repository — Core OS packages                   │
└───────────────────────────────────────────────────────────────┘
```

- **Left border** — 3px, colored: green (healthy), amber (warning), red (error)
- **Status dot** — 8px circle, same color as border
- **Source line** — full `deb` line (row 1)
- **Description** — gray subtitle from knowledge base / Release / domain fallback (row 2)
- **Inline issue** — if warning/error, brief one-line summary in status color after description
- **Toggle** — enable/disable checkbox unchanged, right side

**macOS card:** Same enrichment on QTreeWidget items. Status dot prepended, description as subtitle. Grouped sections remain.

**Theme tokens (new in values.ini):**
- `@repoHealthy` — green status color
- `@repoWarning` — amber status color
- `@repoError` — red status color

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
| Metadata | Status, Last Checked, Packages, File, Suite, Format |
| Issues | List of issues, each with severity icon, summary, detail text, and Repair button (if repairable) |
| Actions | Edit, Open URI, Disable buttons |

**macOS variant:** Same layout adapted — Tap name, Type (formula/cask), Version installed vs available. Actions: Update, Uninstall, Open Homepage.

### Repair Flow (Guided)

1. User clicks "Repair" on an issue in the detail panel
2. Confirmation dialog appears with:
   - Issue summary
   - Exact command that will be run
   - Explanation of what it does and why
3. User confirms → command runs with sudo elevation (Linux) or direct (macOS)
4. On completion, re-checks that specific repo's health
5. Card badge and panel update inline with new status

## Knowledge Base

Static `QMap<QString, RepoKnownInfo>` compiled into the binary. No external file I/O.

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

**Extensibility:** No user-editable entries in v1. Could add JSON override file later if demand warrants.

## Testing Strategy

- **Unit tests** for `RepoHealthChecker` — mock network responses, verify correct issue detection for each check type
- **Unit tests** for `RepoKnowledgeBase` — verify pattern matching, fallback chain
- **Unit tests** for `RepoHealthResult` — verify status aggregation (worst severity wins)
- **Parser tests** — extend existing `test_apt_source_tool.cpp` if data model changes
- **Integration** — manual verification of UI on both platforms

## Scope Boundaries

**In scope:**
- Enriched cards with status and descriptions (Linux + macOS)
- Side detail panel with toggle behavior
- Background health checks via DataRefreshService
- Knowledge base for common repos
- Guided repair for detected issues
- New theme tokens for status colors

**Out of scope (v1):**
- User-editable knowledge base entries
- Persistent health cache across sessions
- Health check notifications outside the page (e.g., system tray)
- Automatic repair without confirmation
- Package-level dependency analysis
