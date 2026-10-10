# App Cleaners (CleanerML)

Nexis can run per-application cleanup rules written in
[CleanerML](https://docs.bleachbit.org/cml/cleanerml.html), the XML format
BleachBit uses. System Cleaner → **Deep clean → App cleaners…** lists the
cleaners for applications that have left data on your computer.

## Where definitions come from

| Source | Location |
|--------|----------|
| Bundled | 104 definitions from BleachBit v6.0.3, compiled into the app (`shared/nexis/cleaners.d/`, GPL-3.0-or-later; licence text ships alongside) |
| Yours | `~/.config/nexis/cleaners.d/*.xml` on Linux, `~/Library/Preferences/nexis/cleaners.d/*.xml` on macOS |

A file of yours whose `<cleaner id="…">` matches a bundled one replaces it.
Definitions are read each time the dialog opens; no restart is needed.

## What Nexis runs, and what it leaves out

Nexis reduces every definition to what it can do completely on the current
platform. On Linux that leaves about 80 of the bundled cleaners (about 210
options); on macOS about 38 (about 95 options), because most definitions only
describe Linux and Windows paths.

Supported actions:

| `command` | `search` | Effect |
|-----------|----------|--------|
| `delete` | `file` (or omitted) | Delete one path |
| `delete` | `glob` | Delete files matching a pattern in one directory |
| `delete` | `walk.files`, `walk.all` | Delete the files under a directory (optionally filtered by `regex`) |
| `truncate` | | Empty a file without deleting it |
| `sqlite.vacuum` | `file`, `glob` | Compact a SQLite database |

Left out:

- **Whole options that need anything else** — `cookie`, `json`, `ini`, `xml`,
  `chrome.*`, `mozilla.*`, `apt.*`, `process`, and `search="deep"` (which walks
  your entire home directory). If one action in an option cannot run, the
  option is not offered at all, so nothing is ever cleaned "halfway". For
  browser history and cookies use **Deep clean → Browser history & cookies…**
  instead.
- **Windows-only content** — `winreg` actions, `%VAR%` and backslash paths,
  and anything marked `os="windows"`.
- **Paths outside your home directory.**

## Variables

A path may use:

- `~`, `$HOME`, `$XDG_CONFIG_HOME`, `$XDG_CACHE_HOME`, `$XDG_DATA_HOME`
- `$$name$$` for a `<var name="name">` defined in the same cleaner. Each
  `<value>` may carry `os="linux|macos|unix"`; several values expand to every
  one, and `<value search="glob">` expands to every entry matching its last
  path component (how "every browser profile" is expressed).

A path that still contains an unknown `$NAME` or `$$name$$` is skipped.

## Safety

- Nothing is pre-selected, and nothing is deleted until you have reviewed the
  itemised list and confirmed. A dry run is available in the same dialog.
- Every target must be inside your home (or cache) directory, and those
  directories themselves can never be a target.
- Paths on your **Exclusion Rules** list are never listed or removed.
- System locations, package-owned files, root-owned files and credential
  stores are refused.
- An option with a `<warning>` is shown as risky and needs an extra
  confirmation.
- An application that is open is listed but cannot be selected.
- App cleaners never run as part of "Scan system", the Maintenance Wizard or
  scheduled cleaning.

## Example

```xml
<?xml version="1.0" encoding="UTF-8"?>
<cleaner id="mytool" os="unix">
  <label>My Tool</label>
  <description>Example</description>
  <running type="exe">mytool</running>
  <var name="base">
    <value os="linux">$XDG_CONFIG_HOME/mytool</value>
    <value os="macos">~/Library/Application Support/MyTool</value>
  </var>
  <option id="cache">
    <label>Cache</label>
    <description>Delete cached downloads</description>
    <action command="delete" search="walk.files" path="$$base$$/cache"/>
  </option>
</cleaner>
```
