# `changelog.d/` — changelog fragments

Every user-visible change adds **one new file here** instead of editing the
shared `## [Unreleased]` block in `CHANGELOG.md`.

## Why

`CHANGELOG.md` was the single most contended file in the repo: every concurrent
PR appended to the same `## [Unreleased]` region, so the first PR to merge left
every other open PR with a textual conflict — 7 of 7 merges over one three-day
window, live-locking the review queue with no substantive disagreement between
any of them (SSO-23951). Distinct filenames cannot conflict, so this directory
removes the collision outright.

## How

Create `changelog.d/<slug>.<type>.md`:

- **`<slug>`** — conventionally the issue id plus a short description,
  e.g. `sso-23853-menubar-health-score`. It only has to be unique; keeping the
  issue id in it makes the fragment self-identifying at release cut.
- **`<type>`** — one of `added`, `changed`, `deprecated`, `removed`, `fixed`,
  `security` (the [Keep a Changelog](https://keepachangelog.com/en/1.1.0/)
  sections).

The file body is the changelog entry itself — plain markdown, **no leading
`- `**. Multi-line bodies are fine; they are emitted as one bullet. Write it the
way the existing `CHANGELOG.md` entries read: what changed, for whom, and the
issue id.

```
$ cat changelog.d/sso-23853-menubar-health-score.added.md
macOS menu-bar monitor (SSO-23853): the optional menu-bar item now shows the
0–100 Dashboard health score instead of raw CPU%/MEM%, and clicking it offers a
one-click "Clean Now" action.
```

## Checking your work

```bash
python3 scripts/changelog_fragments.py lint     # validate names and bodies
python3 scripts/changelog_fragments.py render   # preview the assembled sections
```

CI runs `lint` on every PR.

## At release cut

`RELEASE.md` §1 folds every fragment into `CHANGELOG.md` under the new version
header and deletes them:

```bash
python3 scripts/changelog_fragments.py apply --version X.Y.Z --date YYYY-MM-DD
```

Do not run `apply` in a feature PR.
