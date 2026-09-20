Release cut (SSO-24822): `scripts/gen_doc_stats.py` now also syncs the
`> Last updated: YYYY-MM-DD | Version X.Y.Z` prose header in
`docs/APPLICATION_OVERVIEW.md` and `docs/ARCHITECTURE_REVIEW.md` to the
`CMakeLists.txt` version and today's date. Previously nothing wrote that
header, so `scripts/check_doc_versions.sh` failed on every release cut once
the version bumped, blocking the release PR's required Build checks.
`RELEASE.md` §1 now runs the sync after the `CMakeLists.txt` version bump
(it reads that file) and stages `docs/ARCHITECTURE_REVIEW.md` in the release
commit; `gen_doc_stats.py --check` now ignores the header's date field so it
only fails on a genuinely stale version or stats block, not on the tag push
landing a calendar day after the cut commit.
