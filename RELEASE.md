# Nexis Release Runbook

This document is the canonical, hands-on runbook for cutting a Nexis release. It
is written so a future agent or engineer can ship a release end-to-end without
prior context, and is kept short enough to read in one sitting.

For the long-term ownership rules and time-box that govern *who* ships and *how
often*, see [`docs/MAINTAINER_SOP.md`](docs/MAINTAINER_SOP.md).

---

## 0. Pre-flight invariants

Every release must satisfy these — fail closed if any check fails.

1. **GPL-3.0 compliance.** `LICENSE` must remain GPL-3.0-only, unmodified
   from upstream Stacer attribution + S4 Solutions copyright lines. Nexis is and
   always will be free software; no monetization, ever.
   ```bash
   head -1 LICENSE | grep -F "Linux & macOS System Optimizer"   # sanity
   grep -c "GNU GENERAL PUBLIC LICENSE" LICENSE                  # must be ≥ 1
   grep -c "Version 3" LICENSE                                   # must be ≥ 1
   ```
2. **`CHANGELOG.md` has an entry for the new version.** The release workflow
   parses the section between `## [X.Y.Z]` and the next `## [` heading; missing
   sections produce a degraded "Release X.Y.Z" body.
3. **Working tree clean on `native`.** All work is merged; no in-flight PRs that
   should be in this release.
4. **Issue tracker reconciled.** Paperclip is the system of record (see
   `CLAUDE.md` — the `SSO-NNNN` prefix). All issues that should ship in this
   release are closed in Paperclip; any still-open work is either deferred to
   the next release or has a documented carve-out. The retired
   `scripts/nexis_db.py` / `BUGS.md` / `FEATURE_REQUESTS.md` flow no longer
   applies.
5. **Screenshot baselines green (or explicit waiver).** Per NEX-3381, the
   `ScreenshotTests` suite runs non-blocking in `build.yml` on every push to
   `native` (Linux x64 + macOS; ARM64 Linux is skipped due to a known xvfb
   hang). Before tagging:
   ```bash
   # Latest Build run on the tagged SHA — screenshot step must be green on
   # Linux x64 and macOS, OR you must download the screenshot-diffs-*
   # artifacts, visually confirm every diff is an intended change, and either
   # re-run `Regenerate Screenshot Baselines` (workflow_dispatch) and commit
   # the refreshed PNGs before tagging, or note the waiver in CHANGELOG.md.
   gh run list --workflow build.yml --branch native --limit 1
   gh run view <run-id> --log | grep -E "Screenshot regression tests"
   ```
   Any open page-visual PRs in this release must regenerate the affected
   refs in the same PR; never tag against a known-stale baseline without a
   recorded waiver. WI-19 in the audit remediation plan is the rationale.
6. **Time-box not exceeded for the quarter** (see SOP).

---

## 1. Versioning & tag

Nexis follows [SemVer](https://semver.org/spec/v2.0.0.html). Tags are the single
source of truth for the released version — `release.yml` derives the build
version from `GITHUB_REF_NAME` (`v2.3.4` → `2.3.4`).

- **PATCH** (`2.7.0 → 2.7.1`) — bug fixes and small, backward-compatible
  improvements: an extra column on an existing table, copy/UI polish, a tweak to
  behavior that already exists. No new headline capability.
- **MINOR** (`2.7.x → 2.8.0`) — substantial new features or capabilities,
  additive and backward-compatible: a new page, a new monitoring mode, a new
  dashboard tile type — the kind of thing that leads the release notes.
- **MAJOR** (`2.x.y → 3.0.0`) — breaking changes: removing or renaming a
  public-facing API / CLI flag, changing a persisted settings or data format
  incompatibly. Rare; coordinate via SOP.

The PATCH/MINOR line is a judgment call, not a mechanical one. The test:
**would a user scanning the changelog see this as a new thing they can now do
(MINOR), or as a refinement to something already there (PATCH)?** When the
additive change is incremental polish on an existing surface, PATCH is correct;
reserve MINOR for changes that expand what Nexis does. (Note: dropping a
supported platform is a maintainer scope decision handled via SOP, not an
automatic MAJOR — e.g. v2.6.0 sunset macOS Intel and Ubuntu Noble as a MINOR.)

CVE / security patches: see §6.

### Cutting the tag

```bash
# 1. Update CHANGELOG.md: rename "## [Unreleased]" to "## [X.Y.Z] - YYYY-MM-DD"
#    and add a fresh empty "## [Unreleased]" section above it.
# 2. Bump the CMakeLists.txt project() version (`project(Nexis VERSION X.Y.Z)`
#    at the top of CMakeLists.txt). The tag-driven release pipeline passes
#    -DAPP_VERSION_OVERRIDE=<tag> so published artifacts always match the tag,
#    but non-override builds (untagged developer builds, distro-side rebuilds
#    that don't pass the override, the macOS Info.plist short-version baseline)
#    fall back to PROJECT_VERSION — keep it in sync with the tag.
# 3. Bump the AUR PKGBUILD version (linux/aur/PKGBUILD: pkgver=X.Y.Z, pkgrel=1)
# 4. Add a new debian/changelog entry for X.Y.Z (Linux maintainer email).
#    Note: release.yml's "Sync debian changelog version from tag" step
#    auto-rewrites the top entry's version to match the tag if they differ,
#    so a stale top version is recoverable — but for a clean PPA upload, add
#    a real entry with notes.
git add CHANGELOG.md CMakeLists.txt linux/aur/PKGBUILD linux/debian/changelog
git commit -m "chore(release): X.Y.Z"
git push origin native

# 5. Tag and push (this is the trigger for the entire pipeline)
git tag -a vX.Y.Z -m "Nexis X.Y.Z"
git push origin vX.Y.Z
```

The push of `vX.Y.Z` triggers `release.yml`. Do not create the GitHub Release
manually — the workflow handles it (and deletes any pre-existing release for
that tag, a holdover from the pre-rebrand Stacer artifacts; see
`release.yml: Delete pre-existing release for this tag`).

---

## 2. Build matrix

Defined in `.github/workflows/release.yml`. As of v2.6.x (SSO-4093):

| Job | Runner | Output | Notes |
|---|---|---|---|
| `build-linux (x86_64)` | `ubuntu-24.04` (container `ubuntu:26.04`) | `.deb` (`_ubuntu2604.deb`), AppImage, raw binary `nexis-x86_64` | linuxdeploy + qt plugin (run with `APPIMAGE_EXTRACT_AND_RUN=1` — no `/dev/fuse` in container) |
| `build-linux (arm64)` | `ubuntu-24.04-arm` (container `ubuntu:26.04`) | `.deb` (`_ubuntu2604.deb`), AppImage, raw binary `nexis-arm64` | linuxdeploy aarch64 (same FUSE workaround) |
| `build-macos` | `macos-14` (Apple Silicon) | `nexis.app`, `.dmg` | macdeployqt + notarized |
| `release` | `ubuntu-latest` | GitHub Release, attaches all artifacts | extracts notes from `CHANGELOG.md` |

> **Install-tracking assets (2026-07-05 plan).** The `release` job publishes two
> extra assets so per-channel download counting works
> (see `docs/plans/2026-07-05-installation-tracking-findings-and-plan.md`):
>
> - `Nexis-X.Y.Z-macOS-arm64.brew.dmg` — byte-identical copy of the `.dmg`
>   under a brew-specific name. `homebrew.yml` and the tap cask fetch this
>   copy, so GitHub's per-asset counter splits brew installs from direct
>   `.dmg` downloads.
> - `nexis-X.Y.Z-source.tar.gz` — `git archive` of the tag (same content and
>   `Nexis-X.Y.Z/` prefix as the auto-generated tag tarball, which GitHub does
>   **not** count). The AUR PKGBUILD sources this asset, making every
>   `makepkg`/`yay`/`paru` build a counted download.
>
> **AUR source-URL ordering:** the PKGBUILD's `releases/download/...` source
> URL only resolves for releases that ship the source asset (v2.8.3+). Never
> re-point the AUR package at a tag older than the first release carrying
> `-source.tar.gz` without also flipping its `source=` back to the
> `archive/refs/tags/` URL.
>
> The nightly `install-stats.yml` workflow (cron + `workflow_dispatch`)
> aggregates GitHub Releases, Launchpad PPA, and AUR RPC counts into
> `website/src/data/install-stats.json`, rendered at `/Nexis/stats`.

> **Why a Resolute container (SSO-4093 / SSO-7306).** FW-06 (SSO-3733) raised
> the Qt floor to 6.8 LTS. Noble (Ubuntu 24.04) ships Qt 6.4 via `qt6-base-dev`,
> so the previous bare-runner `build-linux` job no longer satisfies the floor.
> SSO-4093 first moved the build into an `ubuntu:25.04` (Plucky) container;
> SSO-7306 then moved it to **`ubuntu:26.04` (Resolute LTS)** when Plucky
> reached end-of-life (its apt archive moves to `old-releases` and the
> container build breaks). Resolute is an LTS (supported to 2031) and ships
> Qt 6.8+. The same container image is shared by `build.yml`, `codeql.yml`, and
> `screenshot-baselines.yml`. The unified job builds **only** the Resolute
> `.deb` (`_ubuntu2604.deb`) — there is no Noble or Plucky `.deb`. Ubuntu 24.04
> and 25.10 users use the AppImage / AUR / PPA path; 25.04 (Plucky) is EOL.

Downstream-triggered (run on success of `Release`):

- `homebrew.yml` — bumps the `s4solutionsllc/homebrew-nexis` Cask to the new DMG.
- `aur.yml` — publishes the AUR `nexis` package via `KSXGitHub/github-actions-deploy-aur`.
- `ppa.yml` — uploads source packages to the Launchpad PPA for `questing`, `resolute` (SSO-3733 / FW-06 dropped `noble` + `jammy`; SSO-7305 added `resolute`; SSO-7306 dropped `plucky` after it reached end-of-life — Launchpad rejects uploads to obsolete series; see "Ubuntu 24.04 (Noble) dropped" below).

> **Action pins (WI-09 / SSO-3371).** Every `uses:` in `.github/workflows/` is
> pinned to a full commit SHA with a trailing `# vX.Y.Z` comment — *never* a
> bare `@v3` / `@v6` tag. This blocks the `tj-actions/changed-files`-style
> supply-chain attack where a re-pointed upstream tag would otherwise execute
> in a context that holds `AUR_SSH_KEY`, `HOMEBREW_TAP_TOKEN`,
> `CROWDIN_PERSONAL_TOKEN`, or `PPA_GPG_PRIVATE_KEY`. Dependabot
> (`.github/dependabot.yml`, `github-actions` ecosystem, weekly) opens PRs to
> roll the SHAs forward when upstreams cut new releases; review the diff
> before merging.

### Currently supported targets

- Linux x86_64 (Ubuntu 26.04 Resolute and newer / Debian 13+) — `.deb` requires Qt 6.8 and is built on Resolute LTS; 25.10 (Questing) uses the PPA or AppImage; see below
- Linux ARM64 (same matrix, for Pi 5 / Jetson / Graviton)
- AppImage (any glibc-recent Linux, including Ubuntu 22.04/24.04 and all Linux Mint versions)
- macOS Apple Silicon (`arm64`) on macOS 14+ — **macOS Intel (`x86_64`) is sunset (SSO-3733 / FW-06)**, see below
- AUR (Arch + derivatives)
- Launchpad PPA (`questing`, `resolute`; `plucky` dropped at EOL — see "Ubuntu 24.04 (Noble) dropped" below)

> **macOS Intel (`x86_64`) is formally sunset as of v2.6.0 (SSO-3733 / FW-06).** The CI matrix has produced **only** `Nexis-${VERSION}-macOS-arm64.dmg` since the macOS runner moved to `macos-14`; v2.6.0 codifies that as a supported-platform decision rather than an implicit CI artifact. macOS 26 ("Tahoe") is the last macOS to boot on Intel and Rosetta 2 is being phased out; macOS 27 ("Golden Gate") is Apple-silicon-only. No Intel runner will be added; no universal `.dmg` is planned. Intel users on macOS 26 can continue running prior `arm64` builds via Rosetta where supported, but `arm64`-only is the published target going forward.

> **Ubuntu 24.04 (Noble) is no longer a supported PPA series as of v2.6.0 (SSO-3733 / FW-06).** Noble ships Qt 6.4, and the project's Qt floor is now 6.8 LTS (see `CMakeLists.txt`). The `noble` Launchpad source upload is removed from `ppa.yml`; PPA users on 24.04 should upgrade to 25.10 (Questing) or 26.04 (Resolute) — both ship a Qt that satisfies the 6.8 floor. (Ubuntu 25.04 Plucky also satisfied the floor but reached end-of-life and was dropped as a PPA series under SSO-7306.) AppImage and the AUR continue to cover older Ubuntu hosts that build against a newer Qt sysroot.

> **Linux Mint 21.x / 22 and Ubuntu 22.04 / 24.04 users: use the AppImage (GH#185).** The `.deb` package (`_ubuntu2604.deb`) requires `libqt6charts6 ≥ 6.8`, which is not available in Ubuntu 22.04 (Qt 6.2), 24.04 (Qt 6.4), or any current Linux Mint release (Mint 22 "Wilma" is Ubuntu 24.04-based; Mint 21.x is Ubuntu 22.04-based). No `.deb` targeting these distributions is planned because their system Qt does not satisfy the Qt 6.8 floor. **The AppImage bundles Qt and runs on any Linux with glibc 2.35+, including all Ubuntu 22.04+ and Mint 21+ hosts.** Use the `.AppImage` download from the GitHub Release.

> **macOS distribution format is `.dmg`, not `.pkg` (SSO-3733 / FW-06).** Tahoe (macOS 26) skips the first-run XProtect prompt for notarized `.dmg`/`.app` artifacts but **not** for `.pkg` installers, and Tahoe 26.3 saw `.pkg` Gatekeeper rejections in the field. Nexis is a drag-to-`/Applications` `.app` with no install-time launchd / scripting that would justify `.pkg` (the `ScheduleManager` plists are written at runtime to `~/Library/LaunchAgents/`, not at install time). Changing macOS distribution format is a maintainer-only scope decision and must be reopened explicitly — do not silently add a `.pkg` job to `release.yml`. See `docs/MAINTAINER_SOP.md` for the matching policy line.

> **Windows is not a release target today.** The SSOA-31 issue mentions it;
> the actual workflow does not produce Windows artifacts. Adding Windows is a
> separate engineering effort (Qt6 MSVC toolchain, MSI/installer, signing
> identity). Do **not** quietly add a Windows job here — open a feature request
> and budget the work explicitly. If the matrix changes, update this table.

---

## 3. Code signing

### macOS (Developer ID + Notarization)

The `build-macos` job auto-detects whether signing is configured. Required
secrets in the `s4solutionsllc/Nexis` repo:

- `APPLE_CERTIFICATE_P12` — base64 of the Developer ID Application `.p12`.
- `APPLE_CERTIFICATE_PASSWORD` — `.p12` password.
- `KEYCHAIN_PASSWORD` — ephemeral keychain password (any value).
- `APP_STORE_CONNECT_KEY_ID` — App Store Connect API key ID.
- `APP_STORE_CONNECT_ISSUER_ID` — issuer ID.
- `APP_STORE_CONNECT_API_KEY` — `.p8` key contents (multi-line OK).

If **all** Apple secrets are present, the job:

1. Imports the cert into a temporary keychain.
2. Signs `.framework`s and `.dylib`s inside-out, then signs `nexis.app` with
   `--options runtime` and `macos/entitlements.plist`.
3. Builds the `.dmg`, signs it, and submits to `notarytool --wait`.
4. Staples the notarization ticket. Failure here fails the release.

If signing secrets are absent, the job warns and falls back to `codesign -`
(ad-hoc). Ad-hoc builds work locally but Gatekeeper will block first-launch
without right-click → Open. This is acceptable only for personal/dev tags;
**production releases must be signed and notarized.**

### Linux

Linux artifacts are not code-signed. Provenance comes from:

- The GitHub Release page (`s4solutionsllc/Nexis`) as the canonical source.
- The Launchpad PPA's GPG-signed source packages (key in `secrets.PPA_GPG_PRIVATE_KEY`).
- The AUR PKGBUILD `sha256sums` (currently `SKIP`; tighten when
  reproducibility work lands).

---

## 4. Homebrew tap bump

Automatic. `homebrew.yml` triggers on successful completion of the `Release`
workflow when the head branch starts with `v`:

1. Downloads the published brew-specific DMG copy (`.brew.dmg` — same bytes
   as the direct `.dmg`, separate download counter) from the GitHub Release.
2. Computes `sha256sum`.
3. Clones `s4solutionsllc/homebrew-nexis` (token: `secrets.HOMEBREW_TAP_TOKEN`).
4. `sed -i` updates `version` and `sha256` in `Casks/nexis.rb`.
5. Commits and pushes to the tap's default branch.

> **One-time tap change (install tracking):** the sed only rewrites
> `version`/`sha256`, so the cask's `url` stanza must be manually re-pointed
> at the `.brew.dmg` asset name **once**, at the same time as the first
> release that ships it (v2.8.3+). Until both land, brew installs keep
> counting against the direct `.dmg`.

### Manual fallback

If the auto-bump fails (token expired, tap permissions changed, DMG URL drift):

```bash
git clone git@github.com:s4solutionsllc/homebrew-nexis.git
cd homebrew-nexis
VERSION=2.3.4
URL="https://github.com/s4solutionsllc/Nexis/releases/download/v${VERSION}/Nexis-${VERSION}-macOS-arm64.brew.dmg"
# -f makes curl exit non-zero on 4xx/5xx instead of writing the error body
# to disk; --retry 3 handles transient GitHub CDN blips. Without these, a
# 404 page would be hashed and committed to the tap, breaking
# `brew install nexis` for everyone.
curl -fsSL --retry 3 "$URL" -o /tmp/nexis.dmg
# Sanity-check the download before hashing.
SIZE=$(stat -c%s /tmp/nexis.dmg 2>/dev/null || stat -f%z /tmp/nexis.dmg)
[ "$SIZE" -ge 1048576 ] || { echo "DMG too small ($SIZE bytes) — aborting"; exit 1; }
file /tmp/nexis.dmg | grep -Eqi 'html|ascii|empty' && { echo "DMG looks like text/HTML — aborting"; exit 1; }
SHA=$(sha256sum /tmp/nexis.dmg | awk '{print $1}')
sed -i "s/^  version \".*\"/  version \"${VERSION}\"/" Casks/nexis.rb
sed -i "s/^  sha256 \".*\"/  sha256 \"${SHA}\"/" Casks/nexis.rb
git commit -am "Bump Nexis to ${VERSION}"
git push origin main
```

> **Backport caveat:** if you are bumping the cask manually for a backport
> (e.g. `v2.3.5` cut after `v2.4.0` is already published), **do not push** —
> the tap should stay on whatever is currently the latest GA line. The
> automated `homebrew.yml` already refuses to downgrade; mirror that
> behavior here.

Verify with `brew install --cask s4solutionsllc/nexis/nexis` on a clean machine.

---

## 5. Release notes

The release body is built by `release.yml` from `CHANGELOG.md`. Authors do
**not** edit GitHub Release prose directly — edit `CHANGELOG.md` and re-tag if
necessary.

### Template (`CHANGELOG.md` block)

```markdown
## [X.Y.Z] - YYYY-MM-DD

### Added
- **<User-facing feature title> (FR-NN):** One paragraph in plain English. What
  the user can now do, where they find it, and any edge cases or platform
  caveats. Avoid implementation jargon.

### Fixed
- **<User-facing fix title> (BUG-NN):** What the user was experiencing, what is
  now correct, and any conditions that had to be met to hit the bug.

### Changed
- **<Behavior change title> (FR-NN/BUG-NN):** What changed, why, and what users
  should expect to do differently (or "no action needed").

### Security
- **<Advisory ID or short title>:** Severity, what was vulnerable, and what
  users should do (upgrade by version X). Link to GHSA when available. See §6.
```

The release workflow assembles:

```
## What's New
<your CHANGELOG section>
---
### Downloads
<auto-rendered table for x86_64, ARM64, macOS>
---
**Full Changelog:** https://github.com/s4solutionsllc/Nexis/compare/...vX.Y.Z
```

> Keep entries user-facing. Do not paste commit messages or PR titles verbatim.
> The CHANGELOG is rendered on the public website (`pages.yml` rebuild is
> triggered at the end of `release.yml`); a sloppy entry is visible to every
> visitor.

---

## 6. CVE / security-fix expedited path

**Target:** patched build out within **7 calendar days** of a credible disclosure.

### Triggering

Any of the following is treated as a security wake (escalates the maintainer
out of the quarterly cadence — see SOP):

- A GitHub Security Advisory or private vuln report (security@s4solutions.ai or
  the repo's "Report a vulnerability" button) judged credible.
- A public CVE referencing Nexis or a directly-bundled dependency (Qt6 charts,
  Qt6 svg, embedded OpenSSL through Qt) with a working PoC.
- A vendor advisory (Qt, distro) marking remote code execution or local
  privilege escalation in code paths Nexis exercises.
- A finding from the automated detection lane (WI-13): a CodeQL alert on
  `native` flagged High/Critical, a Dependabot advisory against a
  GitHub-Actions or runtime dependency, or a new release-time SHA256
  mismatch on `linuxdeploy` / `linuxdeploy-plugin-qt`.

### Monitoring sources (Qt + third-party)

The 7-day clock starts from the first credible signal. Keep these channels
subscribed to a monitored inbox or the maintainer's pager so a disclosure
does not sit unread:

- **Qt security announcements** — subscribe `security@s4solutions.ai` to the
  `announce@lists.qt-project.org` list
  (<https://lists.qt-project.org/listinfo/announce>) and to the Qt blog
  security tag (<https://www.qt.io/blog/tag/security>). Both carry the
  vendor advisories that feed §6 triggers.
- **GitHub Dependabot alerts** — enabled at the repo level; alerts land in
  the maintainer's GitHub inbox and the `security@s4solutions.ai` route.
- **GitHub Code Scanning (CodeQL) alerts** — surfaced in the repo Security
  tab; High/Critical findings page the maintainer via the same email route.
- **`linuxdeploy` / `linuxdeploy-plugin-qt` releases** — watch the release
  feeds (`https://github.com/linuxdeploy/linuxdeploy/releases.atom`,
  `https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases.atom`).
  When bumping the pinned tag in `.github/workflows/release.yml`, refresh
  the `SHA256` constants alongside it.

Owner: NexisMaintainer. Audit cadence: review the subscription list during
the quarterly release review and again whenever the maintainer of record
changes.

### 7-day clock

| Day | Action |
|---|---|
| 0 | Confirm the report. Acknowledge to reporter within 48h. Open a private security advisory in GitHub. |
| 0–2 | Reproduce. Decide affected versions and whether a patch backport is needed (last 2 minor lines). |
| 2–4 | Land the fix on `native`. Add a regression test where feasible. |
| 4–5 | Cherry-pick to any supported maintenance branch. Cut a patch tag (e.g., `v2.3.4` for a 2.3.x security release). |
| 5–6 | Verify all release-pipeline jobs pass and notarization completes (macOS). Manually sanity-check the AUR + Homebrew bumps. For backports, confirm the new release was **not** marked "Latest" and the Cask/AUR were **not** bumped (see "Backport-safe tagging" below). |
| 6–7 | Publish the GHSA, credit the reporter (with their permission), update `SECURITY.md` and `CHANGELOG.md` `### Security` section, and announce on the website + repo discussions. |

### Severity dial

If a working remote exploit is in the wild, compress to ≤72h and ship the fix
with a minimal regression note in `CHANGELOG.md`. The CEO is the escalation
owner for severe disclosures (see SOP); ping immediately, do not wait for the
next heartbeat.

### Coordinated disclosure

For multi-party issues (Qt upstream, distro packagers), follow the Qt
embargo timeline. Tag and publish on the embargo release date, not before.

### Backport-safe tagging

A backport is any patch tag for an *older* minor line than the currently
published GA — e.g. cutting `v2.3.5` after `v2.4.0` is already out. Without
care, a backport will silently downgrade everyone on Homebrew, AUR, and
any installer that follows GitHub's "Latest" pointer.

The release pipeline now defends against this automatically:

- `release.yml` compares the new tag to the current "Latest" release and
  publishes the backport with `make_latest: legacy`, which leaves the
  existing Latest pointer alone. The backport release is still created and
  its artifacts are still attached — it just isn't promoted to Latest.
- `homebrew.yml` reads the cask's currently published `version` and skips
  the bump when the new tag is older.
- `aur.yml` reads the current `pkgver` from AUR and skips the bump when
  the new tag is older. The `workflow_dispatch` form accepts a `force`
  input for the rare manual resync.

**Operator checklist after pushing a backport tag:**

1. Wait for the `release` job to finish, then confirm on the Releases page
   that the new release is **not** the one labelled "Latest".
2. Confirm `homebrew.yml` and `aur.yml` either skipped the bump (notice
   in the job log) or did not run.
3. If you actually want the backport to become Latest (rare — only if the
   newer line has been retracted), edit the release on GitHub and tick
   "Set as the latest release" by hand. Do **not** re-tag.

If you discover a backport already polluted Homebrew/AUR, the recovery is
a manual republish of the *current GA* version through the same channels
(`workflow_dispatch` on `aur.yml` with `force: true`; manual fallback in
§4 for Homebrew) — do not retag the GA.

---

## 7. Manual run / re-run procedures

### Re-run a failed job

GitHub Actions UI → Workflow run → "Re-run failed jobs". Notarization is the
most common transient failure (Apple infra). If it fails twice, read the
notarytool log printed in the job output before re-running.

### Re-cut a tag (rare)

Only if artifacts must be regenerated and the existing tag is unsafe to keep.

```bash
git tag -d vX.Y.Z
git push origin :refs/tags/vX.Y.Z
gh release delete vX.Y.Z --yes
# Make any fixes, re-tag, re-push.
```

The release workflow already deletes pre-existing releases for the tag, so a
re-tag against the same SHA is generally safe.

### Manual PPA publish

`ppa.yml` accepts `workflow_dispatch` with a `version` input. Use this if the
tag-driven run failed for one of the three series and you need to retry just
that series — note that PPA only accepts each version once per series, so
you'll need to bump `pkgrel` in `linux/debian/changelog` if Launchpad already
accepted that version.

---

## 8. Post-release verification

After the GitHub Release is created, verify within 24 hours:

```bash
VERSION=2.3.4

# 1. Release page exists with all expected artifacts.
gh release view "v${VERSION}" --repo s4solutionsllc/Nexis | grep -E '(deb|AppImage|dmg|nexis-|SHA256)'
# Expect (single Resolute .deb per arch since SSO-4093; Noble/Plucky .debs gone;
# install-tracking assets since the 2026-07-05 plan):
#   2 deb (`_ubuntu2604.deb` x86_64/arm64) + 2 AppImage (x86_64/arm64) +
#   2 raw binaries (nexis-x86_64/nexis-arm64) + 1 dmg + 1 brew.dmg +
#   1 source tarball (nexis-X.Y.Z-source.tar.gz) + 1 SHA256SUMS = 10 artifacts.

# 2. Homebrew Cask was bumped.
curl -fsSL https://raw.githubusercontent.com/s4solutionsllc/homebrew-nexis/main/Casks/nexis.rb \
  | grep -E 'version|sha256'

# 3. AUR PKGBUILD was bumped.
curl -fsSL "https://aur.archlinux.org/cgit/aur.git/plain/PKGBUILD?h=nexis" \
  | grep -E '^pkgver='

# 4. Website release notes rendered.
curl -fsI "https://s4solutionsllc.github.io/Nexis/" | head -5

# 5. macOS DMG opens, app launches, Gatekeeper does not block.
#    (Manual step on a clean macOS machine, or document why skipped.)
```

If any step fails, mark the release `pre-release` on GitHub until resolved and
follow the SOP escalation path.

---

## 9. Dry-run procedure (no public tag)

A dry-run validates the runbook without producing a public artifact. Run before
the first real release after any meaningful change to `release.yml`,
`homebrew.yml`, `aur.yml`, or `ppa.yml`.

1. Branch from `native`: `git checkout -b dry-run/release-vX.Y.Z-rcN`
2. Bump `CHANGELOG.md`, `linux/aur/PKGBUILD`, `linux/debian/changelog` exactly
   as in §1 but with version `X.Y.Z-rcN`.
3. Build locally on Linux and macOS:
   ```bash
   # Linux
   cmake -B build -DCMAKE_BUILD_TYPE=Release \
     -DAPP_VERSION_OVERRIDE=X.Y.Z-rcN
   cmake --build build -j$(nproc)
   ctest --test-dir build --output-on-failure

   # macOS
   cmake -B build -DCMAKE_BUILD_TYPE=Release \
     -DCMAKE_PREFIX_PATH=$(brew --prefix qt@6) \
     -DAPP_VERSION_OVERRIDE=X.Y.Z-rcN
   cmake --build build -j$(sysctl -n hw.ncpu)
   $(brew --prefix qt@6)/bin/macdeployqt build/output/nexis.app -no-strip
   codesign --force --deep --sign - build/output/nexis.app
   ```
4. Walk this runbook end-to-end, **without pushing a tag**, and check off every
   step in `backlog/SSOA-31_dry_run.md` (or a new file for future dry-runs).
5. Note any drift between the runbook and the workflows; fix the runbook.

A dry-run is intentionally cheap. The cost of a real release that exposes a
process bug — broken Cask, missing notarization, malformed changelog — is much
higher because users see it.

---

## 10. Ownership

- **Maintainer of record:** EngineeringLead (per SSOA-31).
- **Escalation owner:** CEO, for security/legal incidents only.
- **Time-box and on-call rules:** [`docs/MAINTAINER_SOP.md`](docs/MAINTAINER_SOP.md).
- **Changes to this runbook:** require maintainer review; bug fixes can be
  committed directly, structural changes go through a PR with a brief rationale.
