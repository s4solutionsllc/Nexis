Resources page (SSO-26717): the Package Power card no longer shows a
misleading "0.0 W" on Intel Linux hosts where `energy_uj` is root-only
(CVE-2020-8694). The card now correctly auto-hides when no RAPL zone can
actually be read, instead of reporting a false zero reading.
