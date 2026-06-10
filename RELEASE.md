# Nexis Release Runbook

This document is the canonical, hands-on runbook for cutting a Nexis release. It
is written so a future agent or engineer can ship a release end-to-end without
prior context, and is kept short enough to read in one sitting.

For the long-term ownership rules and time-box that govern *who* ships and *how
often*, see [`docs/MAINTAINER_SOP.md`](docs/MAINTAINER_SOP.md).

---

## 0. Pre-flight invariants

Every release must satisfy these — fail closed if any check fails.

1. **GPL-3.0 compliance.** `LICENSE` must remain GPL-3.0-or-later, unmodified
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
4. **Tracking files reconciled.** `python scripts/nexis_db.py sync` has been
   run; closed items for this release are `[x]` in `BUGS.md` /
   `FEATURE_REQUESTS.md`.
5. **Time-box not exceeded for the quarter** (see SOP).

---

## 1. Versioning & tag

Nexis follows [SemVer](https://semver.org/spec/v2.0.0.html). Tags are the single
source of truth for the released version — `release.yml` derives the build
version from `GITHUB_REF_NAME` (`v2.3.4` → `2.3.4`).

- **PATCH** (`2.3.3 → 2.3.4`) — bug fixes, no behavior change for users.
- **MINOR** (`2.3.x → 2.4.0`) — new features, additive.
- **MAJOR** (`2.x.y → 3.0.0`) — breaking changes (rare; coordinate via SOP).

CVE / security patches: see §6.

### Cutting the tag

```bash
# 1. Update CHANGELOG.md: rename "## [Unreleased]" to "## [X.Y.Z] - YYYY-MM-DD"
#    and add a fresh empty "## [Unreleased]" section above it.
# 2. Bump the AUR PKGBUILD version (linux/aur/PKGBUILD: pkgver=X.Y.Z, pkgrel=1)
# 3. Add a new debian/changelog entry for X.Y.Z (Linux maintainer email).
#    Note: release.yml's "Sync debian changelog version from tag" step
#    auto-rewrites the top entry's version to match the tag if they differ,
#    so a stale top version is recoverable — but for a clean PPA upload, add
#    a real entry with notes.
git add CHANGELOG.md linux/aur/PKGBUILD linux/debian/changelog
git commit -m "chore(release): X.Y.Z"
git push origin native

# 4. Tag and push (this is the trigger for the entire pipeline)
git tag -a vX.Y.Z -m "Nexis X.Y.Z"
git push origin vX.Y.Z
```

The push of `vX.Y.Z` triggers `release.yml`. Do not create the GitHub Release
manually — the workflow handles it (and deletes any pre-existing release for
that tag, a holdover from the pre-rebrand Stacer artifacts; see
`release.yml: Delete pre-existing release for this tag`).

---

## 2. Build matrix

Defined in `.github/workflows/release.yml`. As of v2.3.x:

| Job | Runner | Output | Notes |
|---|---|---|---|
| `build-linux (x86_64)` | `ubuntu-24.04` | `.deb` (24.04), AppImage, raw binary `nexis-x86_64` | linuxdeploy + qt plugin |
| `build-linux (arm64)` | `ubuntu-24.04-arm` | `.deb` (24.04), AppImage, raw binary `nexis-arm64` | linuxdeploy aarch64 |
| `build-linux-deb-plucky (x86_64)` | `ubuntu-24.04` (container `ubuntu:25.04`) | `.deb` (Plucky / 25.04+) | non-t64 dep names (BUG-75) |
| `build-linux-deb-plucky (arm64)` | `ubuntu-24.04-arm` (container `ubuntu:25.04`) | `.deb` (Plucky / 25.04+) | non-t64 dep names (BUG-75) |
| `build-macos` | `macos-14` (Apple Silicon) | `nexis.app`, `.dmg` | macdeployqt + notarized |
| `release` | `ubuntu-latest` | GitHub Release, attaches all artifacts | extracts notes from `CHANGELOG.md` |

Downstream-triggered (run on success of `Release`):

- `homebrew.yml` — bumps the `s4solutionsllc/homebrew-nexis` Cask to the new DMG.
- `aur.yml` — publishes the AUR `nexis` package via `KSXGitHub/github-actions-deploy-aur`.
- `ppa.yml` — uploads source packages to the Launchpad PPA for `noble`, `jammy`, `questing`.

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

- Linux x86_64 (Ubuntu 24.04 / 24.10 / Debian 12+ / Mint 22)
- Linux x86_64 (Ubuntu 25.04 Plucky and newer)
- Linux ARM64 (same matrix, for Pi 5 / Jetson / Graviton)
- AppImage (any glibc-recent Linux)
- macOS Apple Silicon (M1/M2/M3/M4) on macOS 14+
- AUR (Arch + derivatives)
- Launchpad PPA (`noble`, `jammy`, `questing`)

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

1. Downloads the published macOS DMG from the GitHub Release.
2. Computes `sha256sum`.
3. Clones `s4solutionsllc/homebrew-nexis` (token: `secrets.HOMEBREW_TAP_TOKEN`).
4. `sed -i` updates `version` and `sha256` in `Casks/nexis.rb`.
5. Commits and pushes to the tap's default branch.

### Manual fallback

If the auto-bump fails (token expired, tap permissions changed, DMG URL drift):

```bash
git clone git@github.com:s4solutionsllc/homebrew-nexis.git
cd homebrew-nexis
VERSION=2.3.4
URL="https://github.com/s4solutionsllc/Nexis/releases/download/v${VERSION}/Nexis-${VERSION}-macOS-arm64.dmg"
curl -sL "$URL" -o /tmp/nexis.dmg
SHA=$(sha256sum /tmp/nexis.dmg | awk '{print $1}')
sed -i "s/^  version \".*\"/  version \"${VERSION}\"/" Casks/nexis.rb
sed -i "s/^  sha256 \".*\"/  sha256 \"${SHA}\"/" Casks/nexis.rb
git commit -am "Bump Nexis to ${VERSION}"
git push origin main
```

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

### 7-day clock

| Day | Action |
|---|---|
| 0 | Confirm the report. Acknowledge to reporter within 48h. Open a private security advisory in GitHub. |
| 0–2 | Reproduce. Decide affected versions and whether a patch backport is needed (last 2 minor lines). |
| 2–4 | Land the fix on `native`. Add a regression test where feasible. |
| 4–5 | Cherry-pick to any supported maintenance branch. Cut a patch tag (e.g., `v2.3.4` for a 2.3.x security release). |
| 5–6 | Verify all release-pipeline jobs pass and notarization completes (macOS). Manually sanity-check the AUR + Homebrew bumps. |
| 6–7 | Publish the GHSA, credit the reporter (with their permission), update `SECURITY.md` and `CHANGELOG.md` `### Security` section, and announce on the website + repo discussions. |

### Severity dial

If a working remote exploit is in the wild, compress to ≤72h and ship the fix
with a minimal regression note in `CHANGELOG.md`. The CEO is the escalation
owner for severe disclosures (see SOP); ping immediately, do not wait for the
next heartbeat.

### Coordinated disclosure

For multi-party issues (Qt upstream, distro packagers), follow the Qt
embargo timeline. Tag and publish on the embargo release date, not before.

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
gh release view "v${VERSION}" --repo s4solutionsllc/Nexis | grep -E '(deb|AppImage|dmg|nexis-)'
# Expect: 2 deb (24.04 x86_64/arm64) + 2 deb (Plucky x86_64/arm64) +
#         2 AppImage (x86_64/arm64) + 2 raw binaries + 1 dmg = 9 artifacts.

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
