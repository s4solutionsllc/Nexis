Website /stats page: the nightly install-stats collector had failed every day
since 2026-08-25 because it pushed straight to the ruleset-protected `native`
branch. Snapshots now go to a dedicated `install-stats` data branch that the
site build overlays at deploy time, and Launchpad API fetches retry on timeout.
