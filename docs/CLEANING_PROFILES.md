# Authoring Nexis cleaning profiles (FW-12)

Nexis ships with a built-in set of **cleaning profiles** — declarative JSON
descriptors that tell the System Cleaner where each application stores its
caches, logs, and other disposable state. Profiles are data, not code: adding
support for a new application is a new JSON file, not a recompile.

This document describes the schema and the loader behaviour so contributors
(and end users — see §7 for the per-platform profiles directory) can add
their own profiles.

---

## 1. Where profiles live

Two locations are merged at load time:

1. **Bundled profiles** — shipped inside the Nexis binary as Qt resources at
   `:/cleaning_profiles/linux/*.json` and `:/cleaning_profiles/macos/*.json`.
   The source of truth is `shared/nexis/cleaning_profiles/` in the Nexis
   repository.
2. **User profiles** — read from `QStandardPaths::AppConfigLocation` joined
   with `cleaning_profiles/`, which is `~/.config/nexis/cleaning_profiles/`
   on Linux and `~/Library/Preferences/nexis/cleaning_profiles/` on macOS —
   the two resolve to different directories, not a shared path. Users can
   drop a JSON file here without touching the Nexis install.

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

- **`safe`** — every listed path is a cache, log, or temp location **that the
  owning app rebuilds on its own**, with no action from the user and no loss of
  settings, state, or the ability to work offline. These run on every System
  Cleaner scan.
- **`aggressive`** — anything else: loss would force a rebuild or re-download,
  or removes the only local copy of something (Maven `~/.m2`, Cargo registry
  sources, Gradle downloaded dependencies, NuGet global packages, Yarn berry's
  global cache, Spotify offline downloads, Mail Downloads, Xcode Archives,
  Dropbox `.dropbox.cache`). These are skipped unless the user opted in via the
  System Cleaner's "Allow aggressive profiles" toggle
  (`SettingManager::CleanerAggressiveProfilesEnabled`).

**The rule is `safe` only if the owning app rebuilds it on its own.** When in
doubt, choose `aggressive`. "A download cache" is not enough: a package store
that projects load from (NuGet global packages, Yarn Plug'n'Play zips) or a
folder that holds offline downloads is the only copy, not a cache. A profile
that mixes both kinds of path is split in two rather than labelled by its
riskiest path (see `gradle-cache` / `gradle-dependencies`).

### Machine-checked policy

`shared/nexis/cleaning_profiles/safety_policy.json` encodes the rule and is
checked by `tests/policy/test_cleaning_profile_safety.py` (CTest
`CleaningProfileSafety`, and the `Cleaning profile safety` job in the
`Docs hygiene` workflow):

- **Deny-list** — locations no bundled profile may touch, safe or aggressive:
  credentials (`~/.ssh`, `~/.gnupg`, keyrings, Keychains…), personal folders,
  installed toolchains and SDKs, editor user data, and the roots that hold many
  apps' data (`~/.config`, `~/Library/Caches`…), which may not be deleted or
  wildcarded as `<root>/*`.
- **Dangerous classes** — path patterns for app settings, keys, history,
  databases, installed toolchains and offline data. A `safe` profile path must
  not match any of them; an `aggressive` profile may.

A wildcard in a profile path only matches an identical wildcard in the policy
(`~/.config/*/Cache` is not flagged for `~/.config/gcloud` just because `*` could
expand to it); generic globs are reviewed on the generated page below.

[`WHAT_NEXIS_TOUCHES.md`](WHAT_NEXIS_TOUCHES.md) lists every location the
bundled profiles may delete and every deny-listed location. It is generated:
run `python3 scripts/cleaning_profile_policy.py` after changing a profile or the
policy. CI fails if it is stale.

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
3. Run `python3 scripts/cleaning_profile_policy.py` to regenerate
   [`WHAT_NEXIS_TOUCHES.md`](WHAT_NEXIS_TOUCHES.md), and commit it with the
   profile. Choose `safe` only by the rule in §4.
4. Verify locally with `ctest --test-dir build -R CleaningProfile` (loader
   tests and the safety policy check).
5. If your addition is user-visible, add a `changelog.d/` fragment (see
   `changelog.d/README.md`). A profile you move from `safe` to `aggressive`
   always counts as user-visible.

`tests/managers/test_cleaning_profiles.cpp` covers the schema validator,
the bundled+user merge, glob expansion, and the safe/aggressive gate. New
profiles do not require a new test — the validator runs over every bundled
file at load time and routes malformed files to
`CleaningProfilesService::lastErrors()`, and the safety policy check (§4) runs
over every bundled profile.

## 7. Adding a user profile (end users)

1. Create the profiles directory if it doesn't exist:
   `~/.config/nexis/cleaning_profiles/` on Linux,
   `~/Library/Preferences/nexis/cleaning_profiles/` on macOS.
2. Drop a `.json` file matching the schema. The filename does not have to
   match the `id`.
3. Run a scan from the System Cleaner page — your profile will appear in
   the `APP_PROFILES` category alongside the bundled ones.

A user profile with the same `id` as a bundled profile replaces it
entirely. Use a different `id` if you only want to add paths.

## 8. Worked example: overriding a bundled profile

To extend the bundled `firefox-cache` profile with an extra path, drop the
following into `~/.config/nexis/cleaning_profiles/firefox-cache.json`:

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
