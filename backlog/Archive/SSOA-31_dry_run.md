# SSOA-31 Dry-Run Log

Walkthrough of `RELEASE.md` end-to-end against current `native` (commit on
record at the time of dry-run: `5a9251d feat(packaging): add AUR package and
release automation (FR-91)`). No public tag is pushed.

Date: 2026-04-30
Operator: EngineeringLead (agent run, SSOA-31 implementation)
Reference branch: `paperclip/SSOA-31-release-runbook`

---

## §0 — Pre-flight invariants

| Check | Command | Result |
|---|---|---|
| GPL-3.0 license header present | `head -1 LICENSE` | "Nexis — Linux & macOS System Optimizer and Monitoring" ✓ |
| GPL header text intact | `grep -c "GNU GENERAL PUBLIC LICENSE" LICENSE` | `1` (≥1) ✓ |
| GPL v3 explicit | `grep -c "Version 3" LICENSE` | `1` (≥1) ✓ |
| CHANGELOG has entry for current version | `awk '/^## \[2.3.3\]/{f=1;next}/^## \[/{if(f)exit}f' CHANGELOG.md` | Non-empty (Restore individual dashboard tiles, FR-132) ✓ |
| Working tree clean on `native` | `git status` (before branching) | clean ✓ |
| Tracking files reconciled | n/a (manual) | reviewed; no in-flight items affected ✓ |
| Quarterly time-box not exceeded | manual check against SOP §3 | within budget ✓ |

**Outcome:** all pre-flight checks pass for a hypothetical re-tag of `v2.3.3`.

---

## §1 — Versioning & tag

Walked the script in `RELEASE.md` §1 for a hypothetical `v2.3.4` (no real
push):

- `CHANGELOG.md` rename of `## [Unreleased]` → `## [2.3.4] - 2026-04-30` and
  re-add empty `## [Unreleased]` above. **OK in principle.**
- `linux/aur/PKGBUILD` `pkgver=2.3.4` bump. Currently `pkgver=2.3.3`. **OK.**
- `linux/debian/changelog` new entry. **DRIFT FOUND:** top entry is
  `nexis (2.2.10-0)` but AUR is `2.3.3`. Cause: `release.yml` step
  "Sync debian changelog version from tag" auto-rewrites the version on the
  top entry at release-time, so the PPA upload still gets the correct version
  even when the in-tree changelog is stale. **Action:** runbook now calls
  out the auto-sync (RELEASE.md §1) so future maintainers don't get spooked
  by a stale top entry. PPA quality-of-life improvement (real changelog
  entries instead of synthetic) is a separate doc-debt task and does not
  block a release.
- `git tag -a vX.Y.Z -m "Nexis X.Y.Z"` and `git push origin vX.Y.Z` — **not
  executed** (this is a dry-run; no public tag).

---

## §2 — Build matrix

Cross-checked the runbook table against `.github/workflows/release.yml`:

| Runbook job | Workflow job | Match? |
|---|---|---|
| `build-linux (x86_64)` on `ubuntu-24.04` | lines 16–137 | ✓ |
| `build-linux (arm64)` on `ubuntu-24.04-arm` | matrix entry, lines 22–32 | ✓ |
| `build-linux-deb-plucky (x86_64)` in `ubuntu:25.04` container | lines 142–225 | ✓ |
| `build-linux-deb-plucky (arm64)` in `ubuntu:25.04` container | matrix entry, lines 150–157 | ✓ |
| `build-macos` on `macos-14` | lines 230–392 | ✓ |
| `release` on `ubuntu-latest` | lines 396–562 | ✓ |
| `homebrew.yml` triggered by `Release` success | `homebrew.yml` `workflow_run` filter | ✓ |
| `aur.yml` triggered by `Release` success | `aur.yml` `workflow_run` filter | ✓ |
| `ppa.yml` triggered by tag `v*` (independent path) | `ppa.yml` `on: push: tags: 'v*'` | ✓ (independent of release.yml success — correct per workflow) |

**Windows note in the runbook is accurate.** No Windows job exists in
`release.yml`. Issue mentions Windows as a "currently supported target" but
this is aspirational, not factual. Runbook §2 calls this out as a
discrepancy and instructs future agents not to silently add Windows; an FR
is required.

---

## §3 — Code signing

- macOS Developer ID + notarytool path: secrets list in runbook matches
  `release.yml` references (`APPLE_CERTIFICATE_P12`,
  `APPLE_CERTIFICATE_PASSWORD`, `KEYCHAIN_PASSWORD`,
  `APP_STORE_CONNECT_KEY_ID`, `APP_STORE_CONNECT_ISSUER_ID`,
  `APP_STORE_CONNECT_API_KEY`). ✓
- Ad-hoc fallback path: matches the `if steps.signing.outputs.ENABLED != 'true'`
  branch. ✓
- Linux unsigned (only PPA GPG): matches `secrets.PPA_GPG_PRIVATE_KEY`
  reference in `ppa.yml`. ✓

**No drift.**

---

## §4 — Homebrew tap bump

- Auto-bump path: `homebrew.yml` matches the runbook's description (download
  DMG, sha256, sed Cask, push). ✓
- Manual fallback: snippet in runbook is functionally equivalent to the
  workflow's logic (URL pattern, sha256sum, sed lines). ✓
- Cask repo: `s4solutionsllc/homebrew-nexis`, token
  `secrets.HOMEBREW_TAP_TOKEN`. ✓

**No drift.**

---

## §5 — Release notes

- Verified the AWK extraction logic in `release.yml` against the current
  `CHANGELOG.md` for `2.3.3`. The AWK pattern `/^## \[${TAG_VERSION}\]/` and
  exit on the next `## [` produces a valid block. ✓
- Template in runbook §5 matches the existing CHANGELOG style (Keep a
  Changelog headers, FR-/BUG- suffixes). ✓
- Website rebuild: `release.yml` ends with `gh workflow run pages.yml --ref
  native`. ✓

**No drift.**

---

## §6 — CVE / security expedited path

- `SECURITY.md` does not currently exist in the repo. **Gap surfaced**, not
  blocking this PR (the runbook covers the workflow). Tracked as a
  follow-up — see "Follow-ups" below.
- 7-day SLA: documented; no automation owns the clock, so the maintainer
  must track manually against the wake date. Acceptable for the volume of
  reports this project realistically receives.

---

## §7 — Manual run / re-run procedures

- Re-run failed job: standard GH UI step. ✓
- Re-cut a tag: snippet uses `git push origin :refs/tags/vX.Y.Z` and
  `gh release delete`; matches GitHub's documented behavior and the
  workflow's "Delete pre-existing release for this tag" step. ✓
- Manual PPA dispatch: `ppa.yml` includes `workflow_dispatch` with a
  `version` input. ✓

**No drift.**

---

## §8 — Post-release verification

Artifact count cross-check, expected from `release.yml` upload list (lines
547–556):

```
DEB_X64_PATH               (1)
DEB_X64_PLUCKY_PATH        (2)
APPIMAGE_X64_PATH          (3)
BINARY_X64_PATH            (4)
DEB_ARM64_PATH             (5)
DEB_ARM64_PLUCKY_PATH      (6)
APPIMAGE_ARM64_PATH        (7)
BINARY_ARM64_PATH          (8)
DMG_PATH                   (9)
```

Total **9 artifacts** — runbook §8 says "9 artifacts". ✓

`gh release view` / Homebrew / AUR / website verification commands are
syntactically correct against today's tooling. Not executed against a real
release on this dry-run (no tag pushed).

---

## §9 — Dry-run procedure

Recursive: this very file is the artifact of running §9. Any future
significant change to `release.yml`/`homebrew.yml`/`aur.yml`/`ppa.yml`
should produce a fresh sibling file (`backlog/dry_run_vX.Y.Z-rcN.md`).

---

## §10 — Ownership

- Maintainer of record line in `RELEASE.md` §10 matches `docs/MAINTAINER_SOP.md`
  §1: EngineeringLead, CEO escalation. ✓
- Time-box rule surfaced in `CLAUDE.md` "Maintenance Time-Box" section. ✓

---

## Drift / gaps surfaced and resolved

| # | Drift | Resolution |
|---|---|---|
| 1 | Issue claims Windows is a supported target; workflow does not produce Windows artifacts. | Runbook §2 calls this out and instructs agents not to silently add a Windows job. |
| 2 | `linux/debian/changelog` top entry stale (`2.2.10` vs current `2.3.3`). | Runbook §1 now documents the auto-sync step in `release.yml` so a stale top entry doesn't break the release. PPA quality-of-life (real changelog entries) tracked as follow-up. |
| 3 | Time-box rule was not previously visible to future agents. | Added "Maintenance Time-Box" section to `CLAUDE.md`. |

## Follow-ups (not blocking SSOA-31)

These are recorded here so they don't get lost; they should be opened as
discrete issues if/when the maintainer decides they fit in the time-box.

- **Add `SECURITY.md` at repo root** (private vuln reporting instructions,
  GHSA link, supported versions list). Currently the SOP and RELEASE.md
  cover the *process*; the *public-facing* report path is implicit.
- **Quarterly debian/changelog hygiene** — backfill 2.3.x entries so the PPA
  source package metadata is human-meaningful instead of synthetic.
- **AUR `sha256sums=('SKIP')`** — tighten when reproducibility work lands.
- **Windows target FR** — only if a customer signal justifies the budget
  impact. CEO decision per SOP §5.

## Sign-off

Dry-run completed without exposing a release-blocking process bug. The
two real drift items found were resolved in the same PR by tightening
RELEASE.md prose. Approved to ship the documentation change as-is.
