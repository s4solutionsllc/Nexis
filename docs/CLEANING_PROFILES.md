# Authoring Nexis cleaning profiles (FW-12)

Nexis ships with a built-in set of **cleaning profiles** — declarative JSON
descriptors that tell the System Cleaner where each application stores its
caches, logs, and other disposable state. Profiles are data, not code: adding
support for a new application is a new JSON file, not a recompile.

This document describes the schema and the loader behaviour so contributors
(and end users with a `~/.config/Nexis/cleaning_profiles/` directory) can add
their own profiles.

---

## 1. Where profiles live

Two locations are merged at load time:

1. **Bundled profiles** — shipped inside the Nexis binary as Qt resources at
   `:/cleaning_profiles/linux/*.json` and `:/cleaning_profiles/macos/*.json`.
   The source of truth is `shared/nexis/cleaning_profiles/` in the Nexis
   repository.
2. **User profiles** — read from `~/.config/Nexis/cleaning_profiles/` on
   both Linux and macOS. (More precisely, `QStandardPaths::AppConfigLocation`
   joined with `cleaning_profiles/`.) Users can drop a JSON file here without
   touching the Nexis install.

If a user profile and a bundled profile share the same `id`, the user profile
**replaces** the bundled one entirely. Otherwise, both are loaded.

Only profiles whose `platforms` field matches the running OS are exposed
through `CleanerService::scan(APP_PROFILES)`. An empty or missing `platforms`
list counts as "applies everywhere".

## 2. Schema

```json
{
  "id": "vscode-cache",
  "name": "Visual Studio Code Caches",
  "app": "Visual Studio Code",
  "description": "Editor cache, GPU cache, code cache, and logs.",
  "platforms": ["linux", "macos"],
  "safety": "safe",
  "paths": [
    "$HOME/.config/Code/Cache",
    "$HOME/.config/Code/CachedData",
    "$HOME/.config/Code/Code Cache",
    "$HOME/.config/Code/GPUCache",
    "$HOME/.config/Code/logs"
  ],
  "minAgeDays": 7
}
```

| Field         | Type           | Required | Notes                                                                                                                                     |
|---------------|----------------|----------|-------------------------------------------------------------------------------------------------------------------------------------------|
| `id`          | string         | yes      | Globally unique, slug-style. Two profiles with the same `id` collide — user wins.                                                          |
| `name`        | string         | yes      | Human-readable display name, shown in the cleaner UI.                                                                                     |
| `app`         | string         | no       | Application/family label (e.g. `"JetBrains IDEs"`). Defaults to `name`.                                                                   |
| `description` | string         | no       | One-sentence rationale, surfaced in tooltips.                                                                                             |
| `platforms`   | string[]       | no       | Subset of `["linux", "macos"]`. Empty/missing = both. Any other value is rejected.                                                        |
| `safety`      | string         | no       | `"safe"` (default) or `"aggressive"`. See §4.                                                                                              |
| `paths`       | string[]       | yes      | One or more glob patterns. Must be non-empty.                                                                                              |
| `minAgeDays`  | non-negative integer | no | Skip files modified within the last N days. `0` (default) = any age.                                                                       |

Any other top-level field is ignored — adding metadata is safe forward-compatible.

## 3. Path patterns

`paths` entries are glob patterns, expanded once at scan time.

### Placeholders

| Token              | Expands to                                                                  |
|--------------------|-----------------------------------------------------------------------------|
| `~/` (leading)     | `$HOME/` (rewritten before resolution)                                      |
| `$HOME`            | `QStandardPaths::HomeLocation`                                              |
| `$XDG_CACHE_HOME`  | env or `$HOME/.cache`                                                       |
| `$XDG_CONFIG_HOME` | env or `$HOME/.config`                                                      |
| `$XDG_DATA_HOME`   | env or `$HOME/.local/share`                                                 |
| `$TMPDIR`          | env or `QDir::tempPath()`                                                   |
| `$LIBRARY`         | `$HOME/Library` (macOS convenience)                                         |
| `$ANY_OTHER_VAR`   | live environment value or empty                                             |
| `${VAR}` form      | also supported                                                              |

A placeholder that resolves to the empty string causes that segment to be
**silently skipped** — the profile contributes zero entries from that path
rather than matching every filesystem object. This is the deliberate safe
default.

### Wildcards

`*`, `?`, and character classes `[abc]` are supported in any segment. Nested
wildcards walk left-to-right against the live filesystem, so a pattern like
`$HOME/.config/*/Cache` expands to every immediate-child `Cache` directory
under `~/.config`.

### Age policy

`minAgeDays` is applied to each *expanded entry* by inspecting
`QFileInfo::lastModified()`. A profile that targets per-app cache directories
will typically leave `minAgeDays` at 0; a profile that targets aged build
artifacts (Xcode Archives, Maven `~/.m2`) should set it high enough that an
active project is not wiped.

## 4. Safety classes

Every profile must declare a safety class. The cleaner enforces it:

- **`safe`** — profiles that touch only well-known cache, log, or
  temp-file locations that an application is built to regenerate. These run
  on every System Cleaner scan.
- **`aggressive`** — profiles that touch locations where loss would force a
  rebuild or re-download (Maven `~/.m2`, Cargo registry sources, Mail
  Downloads, Xcode Archives, Dropbox `.dropbox.cache`). These are skipped
  unless the user opted in via the System Cleaner's "Allow aggressive
  profiles" toggle (`SettingManager::CleanerAggressiveProfilesEnabled`).

When in doubt, choose `safe` and document the trade-off in `description`.

## 5. Exclusions

Profile-discovered paths are routed through the same
`CleanerService::isExcluded()` filter as the legacy categories. Anything a
user added to the cleaner's exclusion list survives a profile scan, regardless
of safety class. There is no separate exclusion list per profile — by design.

## 6. Adding a bundled profile (contributors)

1. Drop a new JSON file into
   `shared/nexis/cleaning_profiles/<platform>/<slug>.json` matching the
   schema above. Pick `id` carefully — it's the identifier users will
   override with their own JSON.
2. Register the file in `shared/nexis/cleaning_profiles.qrc` so it's baked
   into the Qt resource bundle.
3. Verify locally with `ctest --test-dir build -R CleaningProfiles`.
4. Update `CHANGELOG.md` under `## [Unreleased]` if your addition is
   user-visible.

`tests/managers/test_cleaning_profiles.cpp` covers the schema validator,
the bundled+user merge, glob expansion, and the safe/aggressive gate. New
profiles do not require a new test — the validator runs over every bundled
file at load time and routes malformed files to
`CleaningProfilesService::lastErrors()`.

## 7. Adding a user profile (end users)

1. Create `~/.config/Nexis/cleaning_profiles/` if it doesn't exist.
2. Drop a `.json` file matching the schema. The filename does not have to
   match the `id`.
3. Run a scan from the System Cleaner page — your profile will appear in
   the `APP_PROFILES` category alongside the bundled ones.

A user profile with the same `id` as a bundled profile replaces it
entirely. Use a different `id` if you only want to add paths.

## 8. Worked example: overriding a bundled profile

To extend the bundled `firefox-cache` profile with an extra path, drop the
following into `~/.config/Nexis/cleaning_profiles/firefox-cache.json`:

```json
{
  "id": "firefox-cache",
  "name": "Firefox Caches (custom)",
  "app": "Mozilla Firefox",
  "platforms": ["linux"],
  "safety": "safe",
  "paths": [
    "$HOME/.cache/mozilla/firefox/*/cache2",
    "$HOME/.cache/mozilla/firefox/*/startupCache",
    "$HOME/.cache/mozilla/firefox/*/safebrowsing",
    "$HOME/.mozilla/firefox/Crash Reports/pending",
    "$HOME/some/extra/path"
  ],
  "minAgeDays": 0
}
```

The bundled file is now ignored entirely — your version is the one that
runs.
