# AUR packaging — CI-managed, do not hand-edit

`PKGBUILD` and `.SRCINFO` in this directory are **managed by CI** and are a
mirror of what is actually published to the AUR (`nexis`). Editing
`pkgver`, `pkgrel`, `sha256sums`, or `.SRCINFO` by hand will drift out of sync
on the next release — that drift is the exact problem this setup was built to
stop.

## How it works

On every release (and on manual `workflow_dispatch` resyncs),
[`.github/workflows/aur.yml`](../../.github/workflows/aur.yml):

1. Skips the bump if the tag is an older backport than the published AUR
   `pkgver` (the version gate — protects AUR users from downgrades).
2. `sed`s the new `pkgver`/`pkgrel` into `PKGBUILD`.
3. Publishes via `KSXGitHub/github-actions-deploy-aur`, which runs
   `updpkgsums` (fills `sha256sums`) and regenerates `.SRCINFO` inside its own
   clone of the AUR git repo.
4. Pulls the authoritative published `PKGBUILD` + `.SRCINFO` back from AUR
   cgit and commits them to `native`, so this directory self-heals.

## What you *can* edit by hand

The **build-affecting body** of `PKGBUILD` — `depends`, `makedepends`,
`options`, the `build()`/`package()` functions, comments. Those are the real
source of truth and feed straight into the published package. Just leave the
machine-generated lines (`pkgver`, `pkgrel`, `sha256sums`) and all of
`.SRCINFO` to CI.

If you change anything in the build body, the next release picks it up
automatically; to apply it immediately, run the **Publish AUR Package**
workflow via `workflow_dispatch` with the current version (use `force: true`
only for a genuine resync).
