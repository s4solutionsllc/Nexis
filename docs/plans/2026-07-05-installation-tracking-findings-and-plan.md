# Installation Tracking — Findings & Implementation Plan

**Date:** 2026-07-05
**Status:** Implemented (pending first post-merge release-cycle verification and the one-time tap `url` change)
**Goal:** Report how many installations Nexis has — a total, plus a breakdown by
installation type (direct `.deb`, AppImage, direct `.dmg`, apt/PPA, brew, AUR).

---

## 1. Findings

### 1.1 Current distribution channels

Verified against the repo's CI workflows:

| Channel | Mechanism | Workflow |
|---|---|---|
| GitHub Releases (direct) | `.deb` (per-arch, Resolute-built), AppImage (x86_64 + aarch64), `.dmg` (macOS arm64) | `.github/workflows/release.yml` |
| apt | Launchpad PPA `ppa:s4solutionsllc/nexis`, series `questing` + `resolute` | `.github/workflows/ppa.yml` |
| brew | Cask in third-party tap `s4solutionsllc/homebrew-nexis` | `.github/workflows/homebrew.yml` |
| AUR | `nexis` package, auto-published on release | `.github/workflows/aur.yml` |
| Source | `git clone` + CMake | n/a |

The app itself makes exactly one network call: a once-per-launch update check
against `https://api.github.com/repos/s4solutionsllc/Nexis/releases/latest`
(`shared/nexis/Pages/Dashboard/dashboard_page.cpp`). GitHub does not expose
per-caller logs for that API, so it cannot be used for counting as-is.

### 1.2 What is countable per channel today

**Direct `.deb` / AppImage — countable now.** The GitHub Releases API reports a
cumulative `download_count` per uploaded asset, so these split out cleanly (and
per-arch, since assets are per-arch).

**apt (PPA) — countable now; best data available.** Launchpad exposes
per-binary download counts via its API
(`archive.getPublishedBinaries()` → `getDownloadCount()` /
`getDailyDownloadTotals()`), broken down by series and version. Because an
installed machine downloads each new version exactly once when it upgrades,
*downloads of the latest version* is a good proxy for the **active** apt
install base.

**brew — currently conflated with direct `.dmg` downloads.** The cask points at
the *same* `Nexis-X.Y.Z-macOS-arm64.dmg` release asset that direct users
download (`homebrew.yml`), so GitHub's counter lumps brew installs and manual
downloads together. Homebrew's own analytics only cover homebrew-core /
homebrew-cask, not third-party taps, so no help there.

**AUR — currently invisible.** The PKGBUILD (`linux/aur/PKGBUILD`) sources the
auto-generated tag tarball (`archive/refs/tags/vX.Y.Z.tar.gz`). GitHub does
**not** count downloads of auto-generated tarballs — only uploaded release
assets — so AUR installs leave no trace anywhere. The AUR itself tracks only
votes and a popularity score, not installs.

### 1.3 Honest limits of download-based counting

- These are **downloads per version**, not cryptographically unique
  installations. One download can serve several machines (rare); CI, mirrors,
  and bots add noise (small at this project's scale).
- "Latest-version downloads" ≈ active installs is strongest for the PPA
  (apt re-downloads on every upgrade) and weakest for AppImage (users may
  keep old versions indefinitely).
- The only way to count literally unique installations is an in-app first-run
  ping (random UUID + a channel constant stamped at package time). That is a
  privacy/product decision — **escalate to CEO per `docs/MAINTAINER_SOP.md`
  before building** (opt-in requirement, GDPR surface, distros patch
  phone-home out). It is explicitly **out of scope** for this plan.

---

## 2. Implementation Plan

Three phases. Phases 1–2 are release plumbing that make brew and AUR
countable; Phase 3 is the collector that produces the numbers.

### Phase 1 — Disambiguate brew from direct `.dmg` (release plumbing)

- [x] `release.yml`: after building the DMG, upload a **second copy** of the
      same file under a brew-specific asset name, e.g.
      `Nexis-${VERSION}-macOS-arm64.brew.dmg`. Same bytes, distinct counter.
- [x] `homebrew.yml`: point the checksum download and the cask `url` template
      at the `.brew.dmg` asset name (checksum is identical, but the workflow
      should fetch the asset it references for the audit-B2 sanity checks).
- [ ] `s4solutionsllc/homebrew-nexis` `Casks/nexis.rb`: update the `url` stanza
      to the `.brew.dmg` name (one-time manual edit or via the next release's
      automated bump — the sed in `homebrew.yml` only rewrites `version`/
      `sha256`, so the `url` template change is a one-time manual PR to the tap).

**Result:** GitHub per-asset counts split `dmg-direct` vs `brew` from the next
release onward.

### Phase 2 — Make AUR visible (release plumbing)

- [x] `release.yml`: create the source tarball (same content as the tag
      archive; `git archive --format=tar.gz --prefix=Nexis-${VERSION}/ v${VERSION}`)
      and upload it as a release asset, e.g. `nexis-${VERSION}-source.tar.gz`.
- [x] `linux/aur/PKGBUILD`: change `source=` from
      `archive/refs/tags/v$pkgver.tar.gz` to
      `releases/download/v$pkgver/nexis-$pkgver-source.tar.gz`, and adjust the
      extract dir prefix if it differs. `aur.yml`'s `updpkgsums: true` already
      regenerates checksums, so no workflow change is needed.
- [ ] Verify one full release cycle: tag → release → AUR publish → `yay -S nexis`
      builds cleanly from the new source URL.

**Result:** every `makepkg`/`yay`/`paru` build downloads a counted asset —
roughly one per machine per version.

**Ordering note:** the PKGBUILD change must land *after* a release that ships
the source asset, or pin the first converted `pkgver` to that release.
Sequence: merge Phase 2's `release.yml` change → cut release → then flip the
PKGBUILD (or land both and cut the release before the next AUR publish fires).

### Phase 3 — Nightly stats collector + dashboard

- [x] New workflow `.github/workflows/install-stats.yml`:
      - `schedule: cron` (daily) + `workflow_dispatch`.
      - **Source 1 — GitHub Releases API:** all releases → per-asset
        `download_count`. Classify by filename: `_ubuntu2604.deb` → `deb-direct`,
        `.AppImage` → `appimage` (split per arch), `.brew.dmg` → `brew`,
        remaining `.dmg` → `dmg-direct`, `-source.tar.gz` → `aur`.
      - **Source 2 — Launchpad API:** anonymous read of
        `api.launchpad.net/devel/~s4solutionsllc/+archive/ubuntu/nexis`
        → `getPublishedBinaries()` → per-binary `getDownloadCount()`, keyed by
        series (`questing`, `resolute`) and version → `apt` bucket.
      - **Source 3 — AUR RPC:** `aur.archlinux.org/rpc/v5/info?arg[]=nexis` →
        `NumVotes`, `Popularity` (supplementary signal only, not a count).
      - Append a dated snapshot to `website/src/data/install-stats.json`
        (running history, one entry per day) and commit with
        `[skip ci]`-safe semantics (GITHUB_TOKEN pushes don't retrigger
        workflows — same pattern as `aur.yml`'s mirror-back step).
- [x] Snapshot schema (per day):
      `{ date, totals: { all, byChannel: { "deb-direct", appimage, "dmg-direct", brew, apt, aur } }, latestVersion: { version, byChannel: {...} }, aur: { votes, popularity } }`
      — `totals` = cumulative downloads; `latestVersion` = the active-install
      proxy.
- [x] Website: a simple stats page/section reading `install-stats.json` —
      headline total, per-channel breakdown, and a latest-version
      ("active installs") view. Follows the existing Astro site structure
      under `website/`.
- [x] Backfill note: GitHub only exposes *cumulative* counts, so the time
      series starts the day the collector first runs; day one (2026-07-05)
      was seeded at implementation time by running the collector locally
      against the live APIs, so `install-stats.json` ships with a real first
      snapshot.

### Acceptance criteria

1. After the first post-merge release, GitHub Releases shows distinct
   `.brew.dmg` and `-source.tar.gz` assets, and the AUR PKGBUILD builds from
   the release asset.
2. `install-stats.yml` runs green daily and grows
   `website/src/data/install-stats.json` by one snapshot per day.
3. The website renders: total downloads, per-channel breakdown
   (deb-direct / appimage / dmg-direct / brew / apt / aur), and the
   latest-version proxy.

### Explicitly out of scope

- In-app telemetry / unique-install UUIDs (CEO decision required — see §1.3).
- Flathub / Snap channels (each would bring native install-base stats if ever
  added; revisit then).

---

## 3. Documentation updates on implementation

Per the pre-commit checklist in `CLAUDE.md`, when Phases 1–3 land:

- `CHANGELOG.md`: entries under the release that ships the new assets and the
  stats collector.
- `docs/APPLICATION_OVERVIEW.md`: no change (no in-app behavior changes).
- `RELEASE.md`: note the two new release assets (`.brew.dmg`, `-source.tar.gz`)
  in the artifact list and the AUR source-URL dependency ordering.
