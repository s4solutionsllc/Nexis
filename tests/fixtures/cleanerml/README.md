# CleanerML fixture corpus (BleachBit import)

`cleaners.d/` is a vendored, unmodified snapshot of BleachBit's community
CleanerML cleaner definitions. It serves two purposes:

1. **Parser test fixtures** — a large, real-world corpus of CleanerML XML for
   exercising Nexis's CleanerML parser (SSO-15366) against the shapes actual
   cleaner authors produce, not just hand-written unit-test snippets.
2. **Default cleaner set** — the source Nexis ships as its built-in
   CleanerML-compatible per-app cleaner definitions (SSO-15366 epic).

## Provenance

| | |
|---|---|
| Source repository | https://github.com/bleachbit/bleachbit |
| Tag pulled | `v6.0.3` |
| Commit | `9e1654fb32460f034cb384e31a2f447c886fedd8` |
| Pulled | 2026-08-30 |
| Upstream path | `cleaners/*.xml` (BleachBit calls this directory `cleaners/`; Nexis mirrors it here as `cleaners.d/` to match the CleanerML spec's own naming convention) |
| File count | 104 `.xml` cleaner definitions |

To refresh: `git clone --filter=blob:none --no-checkout --depth 1 --branch <tag> https://github.com/bleachbit/bleachbit.git`,
sparse-checkout `cleaners`, copy `*.xml` over this directory, and update this
table with the new tag/commit/date/file count.

## License and attribution

Every file in `cleaners.d/` is copyright (c) 2008–2026 Andrew Ziem and the
BleachBit contributors, licensed **GPL-3.0-or-later** (some files carry the
full GPL boilerplate header, others the short `SPDX-License-Identifier:
GPL-3.0-or-later` form — both denote the same license). The full license text
as distributed by BleachBit is vendored alongside as `COPYING.bleachbit`.

Files are imported verbatim, including their original copyright/license
headers — nothing has been stripped or rewritten. This is compatible with
Nexis's own GPL-3.0-only licensing: Nexis distributes these as unmodified
GPL-3.0-or-later data files under their original terms, not as GPL-3.0-only
code.

**Attribution:** cleaner definitions in this directory originate from the
[BleachBit](https://www.bleachbit.org) project and its contributors, not
from Nexis or S4 Solutions.

## Parser contract

Per the SSO-15366 parser ticket's contract: every file in this corpus must
parse without the parser crashing. Errors on individual actions the parser
doesn't yet support are expected and acceptable — they're logged, not fatal.
This corpus is not hand-curated to only contain "supported" syntax; it's the
real upstream set, warts and all, so parser gaps show up here first.

All 104 files were verified well-formed XML (`xml.etree.ElementTree.parse`)
at import time. That's a baseline sanity check only — it does not exercise
Nexis's CleanerML semantics, which is what the SSO-15366 parser's own test
suite covers using this corpus as fixtures.
