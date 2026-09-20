Release cut (SSO-24822): `scripts/gen_doc_stats.py` now also syncs the
`> Last updated: YYYY-MM-DD | Version X.Y.Z` prose header in
`docs/APPLICATION_OVERVIEW.md` and `docs/ARCHITECTURE_REVIEW.md` to the
`CMakeLists.txt` version and today's date. Previously nothing wrote that
header, so `scripts/check_doc_versions.sh` failed on every release cut once
the version bumped, blocking the release PR's required Build checks.
