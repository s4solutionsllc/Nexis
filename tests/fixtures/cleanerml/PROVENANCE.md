# CleanerML parser test fixtures

Ten of the files in this directory (`adobe_reader.xml`, `apt.xml`, `bash.xml`,
`deepscan.xml`, `firefox.xml`, `gimp.xml`, `journald.xml`, `sqlite3.xml`,
`thunderbird.xml`, `vlc.xml`) are unmodified copies of real-world CleanerML
cleaner definitions from the [BleachBit](https://www.bleachbit.org) project
(`bleachbit/bleachbit`, tag `v6.0.3`), copyright (c) 2008-2026 Andrew Ziem and
the BleachBit contributors, licensed GPL-3.0-or-later — see each file's own
header. They're a small, hand-picked subset chosen to exercise every action
type the SSO-23856 `CleanerML::parseXml`/`parseFile`/`parseDirectory` parser
supports, plus several that intentionally fail parsing because they use
BleachBit commands outside this parser's scope (`cookie`, `mozilla.*`,
`apt.*`, `journald.clean`, `ini`) — that's expected: those cleaners are
dropped with a logged `ParseError`, not a crash.

The full 104-file BleachBit corpus (used as both a broader parser regression
set and Nexis's shipped default cleaner definitions, with the complete
`COPYING.bleachbit` license text) is vendored separately under
`cleaners.d/` by SSO-23858; nothing here duplicates that effort.

`malformed.xml` is Nexis-authored (not from BleachBit) — deliberately invalid
XML used to test the parser's syntax-error path.
