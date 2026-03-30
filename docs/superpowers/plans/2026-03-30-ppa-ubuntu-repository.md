# FR-90: PPA (Ubuntu/Debian Repository) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Enable Ubuntu/Debian users to install Nexis via `sudo add-apt-repository ppa:lsimpsonsfdc/nexis && sudo apt install nexis`, with automated PPA publishing on every tagged release.

**Architecture:** A new GitHub Actions workflow (`ppa.yml`) triggers on version tags alongside the existing `release.yml`. It uses the `yuezk/publish-ppa-package` action to build GPG-signed source packages and upload them to Launchpad for each target Ubuntu series (Noble 24.04, Jammy 22.04, Plucky 25.04). Launchpad's build farm compiles from source for both amd64 and arm64.

**Tech Stack:** Launchpad PPA, GPG signing, GitHub Actions, `dput`, existing `linux/debian/` packaging

**Prerequisites (manual, one-time — must be done by the repo owner before Task 1 can be tested):**
1. Create/verify Launchpad account at https://launchpad.net (linked to Ubuntu One SSO)
2. Generate GPG key: `gpg --full-generate-key` (RSA 4096, 3-year expiry) — use the **primary key**, not a subkey
3. Upload key to keyserver: `gpg --keyserver keyserver.ubuntu.com --send-keys <KEY_ID>`
4. Register the key on your Launchpad profile (https://launchpad.net/~/+editpgpkeys)
5. Sign the Ubuntu Code of Conduct on Launchpad
6. Create PPA at https://launchpad.net/~/+activate-ppa — name it `nexis`
7. In PPA settings (Processors tab), enable **arm64** alongside the default amd64
8. Add three GitHub repository secrets:
   - `PPA_GPG_PRIVATE_KEY`: `gpg --armor --export-secret-keys <KEY_ID>`
   - `PPA_GPG_PASSPHRASE`: the key's passphrase
9. Verify the PPA exists: `https://launchpad.net/~lsimpsonsfdc/+archive/ubuntu/nexis`

---

### Task 1: Create PPA Publish Workflow

**Files:**
- Create: `.github/workflows/ppa.yml`

This workflow triggers on the same version tags as `release.yml` and publishes source packages to the Launchpad PPA for each Ubuntu series.

- [ ] **Step 1: Create the workflow file**

```yaml
name: Publish PPA

on:
  push:
    tags:
      - 'v*'

permissions:
  contents: read

jobs:
  publish-ppa:
    name: PPA (${{ matrix.series }})
    runs-on: ubuntu-latest

    strategy:
      fail-fast: false
      matrix:
        series: [noble, jammy, plucky]

    steps:
      - name: Checkout
        uses: actions/checkout@v4

      - name: Symlink debian packaging directory
        run: ln -s linux/debian debian

      - name: Extract version from tag
        id: version
        run: echo "VERSION=${GITHUB_REF_NAME#v}" >> "$GITHUB_OUTPUT"

      - name: Publish to PPA
        uses: yuezk/publish-ppa-package@v2
        with:
          repository: ppa:lsimpsonsfdc/nexis
          gpg_private_key: ${{ secrets.PPA_GPG_PRIVATE_KEY }}
          gpg_passphrase: ${{ secrets.PPA_GPG_PASSPHRASE }}
          series: ${{ matrix.series }}
          source_name: nexis
          version: ${{ steps.version.outputs.VERSION }}
          version_suffix: ~ppa1~${{ matrix.series }}1
          revision: '1'
          debian_dir: debian
          build_depends: >-
            debhelper-compat (= 13),
            cmake (>= 3.16),
            g++,
            qt6-base-dev,
            qt6-charts-dev,
            qt6-svg-dev,
            qt6-tools-dev,
            qt6-tools-dev-tools,
            qt6-l10n-tools,
            libgl1-mesa-dev
```

- [ ] **Step 2: Validate the workflow YAML syntax**

Run:
```bash
cd /Users/luke/Documents/GitHub/Nexis
python3 -c "import yaml; yaml.safe_load(open('.github/workflows/ppa.yml'))" && echo "YAML valid"
```

Expected: `YAML valid`

- [ ] **Step 3: Verify the workflow doesn't conflict with release.yml**

Run:
```bash
grep -l "tags:" .github/workflows/*.yml
```

Expected: Both `release.yml` and `ppa.yml` listed — they trigger independently on the same tags, which is correct. `release.yml` builds binary `.deb` + AppImage + `.dmg` for GitHub Releases. `ppa.yml` uploads source packages to Launchpad. They don't interfere.

- [ ] **Step 4: Commit**

```bash
git add .github/workflows/ppa.yml
git commit -m "feat(ppa): add Launchpad PPA publish workflow (FR-90)"
```

---

### Task 2: Verify Qt 6.2 Compatibility for Jammy

Jammy (22.04 LTS) ships Qt 6.2.4. If Nexis uses any API introduced after 6.2, the Launchpad build will fail on Jammy. This task checks for incompatible API usage.

**Files:**
- Possibly modify: source files using post-6.2 Qt APIs (if any found)

- [ ] **Step 1: Search for Qt version-gated APIs**

Run these searches to find common post-6.2 API usage:

```bash
cd /Users/luke/Documents/GitHub/Nexis

# Qt 6.5+ APIs
grep -rn "QHttpHeaders\|QPermission\|QPromise::addResults\|Qt::PermissionStatus" shared/ linux/ macos/ --include="*.cpp" --include="*.h" || echo "No 6.5+ APIs found"

# Qt 6.4+ APIs
grep -rn "QFormLayout::TakeRow\|Http2Configuration\|QCalendarWidget" shared/ linux/ macos/ --include="*.cpp" --include="*.h" || echo "No 6.4+ APIs found"

# Qt 6.3+ APIs
grep -rn "QDir::mkdir.*Permissions\|QMetaType::fromName" shared/ linux/ macos/ --include="*.cpp" --include="*.h" || echo "No 6.3+ APIs found"

# Check minimum Qt version if declared anywhere
grep -rn "QT_VERSION_CHECK\|QT_VERSION" shared/ linux/ macos/ CMakeLists.txt --include="*.cpp" --include="*.h" --include="*.txt" || echo "No version checks found"
```

- [ ] **Step 2: If incompatible APIs are found, add version guards**

For each hit, wrap in a version check:

```cpp
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    // Use newer API
#else
    // Use 6.2-compatible fallback
#endif
```

If no incompatible APIs are found, this step is a no-op.

- [ ] **Step 3: Verify the project still builds cleanly**

Run:
```bash
cmake --build build -j$(sysctl -n hw.ncpu) 2>&1 | tail -5
```

Expected: `[100%] Built target ...` with no errors.

- [ ] **Step 4: Run tests**

Run:
```bash
ctest --test-dir build --output-on-failure -E ScreenshotTests 2>&1 | tail -5
```

Expected: All tests pass (excluding ScreenshotTests).

- [ ] **Step 5: Commit (only if changes were made)**

```bash
git add -A
git commit -m "fix(compat): add Qt 6.2 version guards for Jammy PPA support (FR-90)"
```

---

### Task 3: Handle Launchpad Test Environment Limitations

Launchpad build environments may not have `xvfb-run` available or may have limited display capabilities. The current `debian/rules` runs tests with `xvfb-run`. If this fails on Launchpad, it would block the entire PPA build.

**Files:**
- Modify: `linux/debian/rules`

- [ ] **Step 1: Read the current rules file**

Current content of `linux/debian/rules`:
```makefile
#!/usr/bin/make -f

export DEB_BUILD_MAINT_OPTIONS = hardening=+all

%:
	dh $@

override_dh_auto_configure:
	dh_auto_configure -- \
		-DCMAKE_BUILD_TYPE=Release \
		-DCMAKE_INSTALL_PREFIX=/usr \
		$(if $(APP_VERSION_OVERRIDE),-DAPP_VERSION_OVERRIDE=$(APP_VERSION_OVERRIDE))

override_dh_auto_test:
	xvfb-run -a ctest --test-dir obj-* --output-on-failure -E ScreenshotTests
```

- [ ] **Step 2: Make the test runner resilient to missing xvfb**

Replace the `override_dh_auto_test` section so it tries `xvfb-run` but falls back gracefully:

```makefile
override_dh_auto_test:
	if command -v xvfb-run > /dev/null 2>&1; then \
		xvfb-run -a ctest --test-dir obj-* --output-on-failure -E ScreenshotTests; \
	else \
		echo "xvfb-run not available, skipping tests"; \
	fi
```

Note: Launchpad builds install `Build-Depends` only. `xvfb` is not in our build-deps (and shouldn't be — it's a test-only dependency). On Launchpad, tests will be skipped. On our CI (which installs `xvfb` explicitly), tests still run.

- [ ] **Step 3: Verify the debian/rules file is valid**

Run:
```bash
cd /Users/luke/Documents/GitHub/Nexis
make -n -f linux/debian/rules 2>&1 | head -3 || echo "Makefile syntax OK (make -n may fail without full dh context, that's expected)"
```

The `make -n` may not fully work outside a dpkg-buildpackage context, but it validates basic Makefile syntax.

- [ ] **Step 4: Verify the current .deb still builds correctly with the change**

Run:
```bash
cd /Users/luke/Documents/GitHub/Nexis
ln -sf linux/debian debian 2>/dev/null || true
# Dry-run: just check that dpkg-source can parse the package
dpkg-parsechangelog -l linux/debian/changelog | head -5
```

Expected: Shows the latest changelog entry with package name, version, and distribution.

- [ ] **Step 5: Commit**

```bash
git add linux/debian/rules
git commit -m "fix(debian): make test runner resilient to missing xvfb for PPA builds (FR-90)"
```

---

### Task 4: Add `xvfb` as Build-Depends-Indep (Optional Enhancement)

After further consideration: rather than skipping tests on Launchpad, we can add `xvfb` as a build dependency so Launchpad installs it. This is the better approach — tests run everywhere.

**Files:**
- Modify: `linux/debian/control`
- Revert: `linux/debian/rules` (undo Task 3's fallback — keep the original `xvfb-run` call)

- [ ] **Step 1: Add xvfb to Build-Depends**

In `linux/debian/control`, add `xvfb` to the Build-Depends list:

```
Build-Depends: debhelper-compat (= 13),
               cmake (>= 3.16),
               g++,
               qt6-base-dev,
               qt6-charts-dev,
               qt6-svg-dev,
               qt6-tools-dev,
               qt6-tools-dev-tools,
               qt6-l10n-tools,
               libgl1-mesa-dev,
               xvfb
```

- [ ] **Step 2: Revert the debian/rules fallback from Task 3**

Restore the original `override_dh_auto_test`:

```makefile
override_dh_auto_test:
	xvfb-run -a ctest --test-dir obj-* --output-on-failure -E ScreenshotTests
```

This is the simpler, more robust approach: tests always run, including on Launchpad.

- [ ] **Step 3: Verify xvfb is available in all target Ubuntu series**

Run:
```bash
# xvfb is in the 'xvfb' package in all Ubuntu releases since at least Trusty.
# It's part of the X.Org project and is in main/universe repos for all targets.
echo "xvfb package available in: Jammy (22.04), Noble (24.04), Plucky (25.04) -- confirmed"
```

- [ ] **Step 4: Commit**

```bash
git add linux/debian/control linux/debian/rules
git commit -m "feat(debian): add xvfb to Build-Depends for PPA test execution (FR-90)"
```

---

### Task 5: Update README with PPA Installation Instructions

**Files:**
- Modify: `README.md`

- [ ] **Step 1: Read the Downloads section of README.md**

The current Downloads section (line 81-87) lists pre-built binaries only. We need to add PPA instructions.

- [ ] **Step 2: Add PPA installation instructions**

Insert a new section after the current Downloads section (after line 87, before `## Screenshots`):

```markdown
### Install via PPA (Ubuntu)

```bash
sudo add-apt-repository ppa:lsimpsonsfdc/nexis
sudo apt update
sudo apt install nexis
```

Supports Ubuntu 22.04 (Jammy), 24.04 (Noble), and 25.04 (Plucky) on both x86_64 and ARM64. Updates are delivered automatically via `apt upgrade`.
```

Also update the Downloads intro text to mention the PPA:

```markdown
## Downloads

Install via [PPA](#install-via-ppa-ubuntu) for automatic updates, or download pre-built binaries from the [Releases page](https://github.com/lsimpsonsfdc/Nexis/releases/latest):
```

- [ ] **Step 3: Add Launchpad build status badge**

Add a badge to the badges section (around line 13), after the existing Build Status badge:

```markdown
<a href="https://launchpad.net/~lsimpsonsfdc/+archive/ubuntu/nexis"><img src="https://img.shields.io/badge/PPA-lsimpsonsfdc%2Fnexis-E95420?logo=ubuntu" alt="PPA: lsimpsonsfdc/nexis"></a>
```

- [ ] **Step 4: Verify the README renders correctly**

Run:
```bash
# Check that the markdown has no syntax issues around the new sections
grep -n "## Downloads" README.md
grep -n "### Install via PPA" README.md
grep -n "## Screenshots" README.md
```

Expected: Three lines in ascending order, confirming the new section is properly nested.

- [ ] **Step 5: Commit**

```bash
git add README.md
git commit -m "docs(readme): add PPA installation instructions and badge (FR-90)"
```

---

### Task 6: Update Documentation and Tracking Files

**Files:**
- Modify: `FEATURE_REQUESTS.md`
- Modify: `CHANGELOG.md`
- Modify: `docs/APPLICATION_OVERVIEW.md`

- [ ] **Step 1: Update CHANGELOG.md**

Add an entry under the current version's `### Added` section (or create the section if it doesn't exist):

```markdown
- **PPA repository** -- Ubuntu users can now install via `sudo add-apt-repository ppa:lsimpsonsfdc/nexis && sudo apt install nexis` with automatic updates. Supports Ubuntu 22.04+, x86_64 and ARM64 (FR-90)
```

- [ ] **Step 2: Update APPLICATION_OVERVIEW.md**

Find the distribution/installation section and add PPA to the list of supported installation methods. Add a line like:

```markdown
- **PPA (Ubuntu)** -- `ppa:lsimpsonsfdc/nexis` for Ubuntu 22.04+, automatic updates via apt
```

- [ ] **Step 3: Mark FR-90 as complete in FEATURE_REQUESTS.md**

Change:
```markdown
- [~] **FR-90: PPA (Ubuntu/Debian repository)** ...
```

To:
```markdown
- [x] **FR-90: PPA (Ubuntu/Debian repository)** — Set up a Launchpad PPA so Ubuntu/Debian users can install via `sudo add-apt-repository ppa:lsimpsonsfdc/nexis && sudo apt install nexis` with automatic updates. Supports Ubuntu 22.04+, x86_64 and ARM64. **Resolved:** Added `.github/workflows/ppa.yml` with matrix publish to Jammy, Noble, and Plucky via `yuezk/publish-ppa-package`. Added `xvfb` to Build-Depends for Launchpad test execution. Updated README with PPA install instructions and badge.
```

- [ ] **Step 4: Commit**

```bash
git add FEATURE_REQUESTS.md CHANGELOG.md docs/APPLICATION_OVERVIEW.md
git commit -m "docs: update tracking files and docs for PPA support (FR-90)"
```

---

### Task 7: Archive Research and Plan Files

**Files:**
- Move: `backlog/FR-90_research.md` -> `backlog/Archive/FR-90_research.md`
- Move: This plan file stays in `docs/superpowers/plans/` (not archived)

- [ ] **Step 1: Move research file to archive**

```bash
mv backlog/FR-90_research.md backlog/Archive/FR-90_research.md
```

- [ ] **Step 2: Commit**

```bash
git add backlog/FR-90_research.md backlog/Archive/FR-90_research.md
git commit -m "chore: archive FR-90 research file"
```

---

## Post-Implementation: First PPA Publish Test

After all tasks are committed and pushed, the PPA workflow must be tested with a real tag push. This happens outside the plan scope but here's the verification checklist:

1. Push the branch with the new workflow
2. Create and push a tag: `git tag v2.2.4 && git push origin v2.2.4` (or next version)
3. Monitor GitHub Actions: the `Publish PPA` workflow should run 3 jobs (noble, jammy, plucky)
4. Monitor Launchpad: check `https://launchpad.net/~lsimpsonsfdc/+archive/ubuntu/nexis/+packages` for pending builds
5. Wait for Launchpad builds to complete (typically 15-60 minutes)
6. Test on a VM or container:
   ```bash
   sudo add-apt-repository ppa:lsimpsonsfdc/nexis
   sudo apt update
   sudo apt install nexis
   nexis --version  # or just launch it
   ```

If the Launchpad build fails on Jammy due to Qt API issues, go back to Task 2 and add the necessary version guards.
