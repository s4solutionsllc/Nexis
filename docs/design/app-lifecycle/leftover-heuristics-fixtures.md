# Leftover-Scan Heuristics: Fixture Suite & Precision/Recall Contract

**Issue:** SSO-15387 (sub-item of SSO-15363)
**Consumers:** SSO-15384 (macOS uninstaller), SSO-15385 (Linux uninstaller), SSO-15386 (orphan scanner)
**CISO gate:** Coordinate false-positive tolerance bar with SSO-15373

---

## Purpose

This document describes the fixture corpus and automated precision/recall
harness that the leftover-scan implementations in SSO-15384/15385/15386 must
pass before merge.  The fixtures live alongside the test files so CI enforces
the contract without requiring a real installed-app inventory or root
privileges.

---

## Thresholds

| Scan path | Recall | Precision |
|-----------|--------|-----------|
| macOS bundle-id path (SSO-15384) | 1.0 | ≥ 0.95 |
| Linux XDG name-match path (SSO-15385) | 1.0 | ≥ 0.95 |
| Orphan scanner — Linux XDG (SSO-15386) | 1.0 | ≥ 0.90 |
| Orphan scanner — macOS Library (SSO-15386) | 1.0 | ≥ 0.90 |

Precision is relaxed to 0.90 for the orphan scanner because the name-matching
space is inherently noisier (shared vendor dirs, group containers) and
SSO-15386 is expected to add a suppression allowlist post-MVP.

Recall must always equal 1.0: a missed orphan or leftover is a data-hygiene
failure that erodes user trust.

---

## Fixture Files

### macOS — `tests/core/test_app_leftovers_macos.cpp`

Tests the `findAppLeftovers(Package)` virtual method (implemented in
`macos/nexis-core/Tools/package_tool.cpp`) via a `TestablePackageToolMacOS`
subclass that roots the scan at a `QTemporaryDir` instead of `~/Library`.

#### Scan locations (true-positive fixture set)

| Location | Example artifact |
|----------|-----------------|
| `~/Library/Application Support/<bundleId>` | directory |
| `~/Library/Caches/<bundleId>` | directory |
| `~/Library/Preferences/<bundleId>.plist` | file |
| `~/Library/Logs/<bundleId>` | directory |
| `~/Library/Containers/<bundleId>` | directory |
| `~/Library/Saved Application State/<bundleId>.savedState` | directory |
| `~/Library/LaunchAgents/<bundleId>.plist` | file |

`/Library/LaunchDaemons` is **not** in scope for the user-space scanner
(requires root; privilege boundary documented in
`findAppLeftovers_matchesLaunchDaemons`).

#### True-negative fixture set (precision guards)

| Scenario | Decoy artifact | Expected result |
|----------|---------------|-----------------|
| Different bundle-id | `com.example.OtherApp` | not returned |
| Partial substring (no dot extension) | `com.example.AppExtra` | not returned |
| Shorter vendor prefix | `com.acme` when target is `com.acme.App` | not returned |
| Similar-but-distinct product | `com.vendor.AppPro` when target is `com.vendor.App` | not returned |

#### Boundary documentation

- **Product-name-only dirs** (`PhotoEditor` instead of `com.acme.PhotoEditor`):
  the bundle-id scan does NOT return these.  A secondary name-based scan is
  the responsibility of SSO-15384.
- **Shared vendor dirs** (`ACME Corp`): not returned by bundle-id scan.
- **`Saved Application State` suffix**: `<bundleId>.savedState` matches the
  `startsWith(bundleId + '.')` predicate and is returned correctly.

---

### Linux — `tests/core/test_app_leftovers_linux.cpp`

Tests `LinuxLeftoverScanner::scan(pkgName, fakeHome)` (a stub of the
contract that SSO-15385 will implement on `PackageToolLinux::findAppLeftovers`).
Runs cross-platform (all paths synthetic).

#### Scan locations (true-positive fixture set)

| Location | Example artifact |
|----------|-----------------|
| `~/.config/<pkgName>` | directory |
| `~/.cache/<pkgName>` | directory |
| `~/.local/share/<pkgName>` | directory |
| `~/.config/autostart/<pkgName>.desktop` | file |

#### Package manager types covered

| Type | Example fixture |
|------|----------------|
| dpkg/apt | `vlc`, `dropbox` |
| rpm | `libreoffice`, `code` |
| Flatpak (reverse-domain name) | `org.videolan.VLC`, `com.spotify.Client` |
| Snap | `firefox`, with channel-suffix collision (`firefox-esr`) |

#### True-negative fixture set

| Scenario | Decoy | Expected |
|----------|-------|----------|
| `lib`-prefixed dpkg dep | `libvlc5` when target is `vlc` | not returned |
| Similar name (dash suffix) | `code-insiders` when target is `code` | not returned |
| Shared vendor dir | `JetBrains` when scanning for `idea` | not returned |
| Substring name | `gitkraken` when target is `git` | not returned |
| Similar-but-distinct | `gnome-terminal-server` when target is `gnome-terminal` | not returned |

---

### Orphan scan — `tests/core/test_orphan_scan_fixtures.cpp`

Tests `OrphanScanner::scanXdgOrphans` and `scanLibraryOrphans` (stubs for
the contract that SSO-15386 will implement).  The scanner takes:
- `installedNames` / `installedBundleIds`: the set of currently-installed
  package names / bundle-ids
- `fakeHome`: root of the synthetic FS tree

and returns paths whose dir-name matches NO installed name.

#### True-orphan fixtures

- Linux: dirs under `~/.config`, `~/.cache`, `~/.local/share` whose name is
  absent from the installed set.
- macOS: dirs/files under `~/Library/{Application Support,Caches,…}` whose
  base name (after stripping `.plist`/`.savedState`) is absent.

#### Near-miss (shared-vendor) fixtures

These cases are **not** fully handled by the simple exact-name-match
algorithm.  The tests call `QWARN()` when a near-miss is flagged, documenting
the gap for SSO-15386's post-MVP suppression allowlist:

- Linux: `JetBrains` dir shared by `idea` (uninstalled) and `pycharm` (installed)
- macOS: `com.adobe.shared` group container shared across Adobe products

#### Precision/recall parameters

| Set | True positives | Decoys | Thresholds |
|-----|---------------|--------|------------|
| Linux XDG | 3 orphan dirs | 2 installed dirs | recall=1.0, prec≥0.90 |
| macOS Library | 2 orphan bundle-id dirs | 1 installed dir | recall=1.0, prec≥0.90 |

---

## CI Integration

The fixture tests are registered in `tests/CMakeLists.txt` via
`add_nexis_test()` and run as part of the standard `ctest` suite:

| Test target | Gate for |
|-------------|---------|
| `AppLeftoversMacOSTests` | SSO-15384 merge acceptance |
| `AppLeftoversLinuxTests` | SSO-15385 merge acceptance |
| `OrphanScanFixtureTests` | SSO-15386 merge acceptance |

`AppLeftoversMacOSTests` is Apple-gated in CMake (the `PackageToolMacOS`
header is only present on Darwin).  `AppLeftoversLinuxTests` and
`OrphanScanFixtureTests` are cross-platform (all paths synthetic).

To run locally:

```sh
# From the build directory:
ctest -R "AppLeftovers|OrphanScan" --output-on-failure
```

---

## False-Positive Tolerance (SSO-15373 CISO gate)

The precision thresholds above (≥ 0.95 for uninstaller paths, ≥ 0.90 for
orphan scanner) are the pre-CISO defaults.  Once SSO-15373 posts its CISO
controls policy, update this table and the `QVERIFY2` threshold values in the
test files if the policy requires tighter bounds.

The near-miss `QWARN()` calls in `test_orphan_scan_fixtures.cpp` should be
converted to `QVERIFY(!orphansContainsNearMiss)` once SSO-15386 implements
the vendor-group suppression allowlist and the CISO gate is satisfied.
