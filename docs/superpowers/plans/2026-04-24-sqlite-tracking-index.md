# SQLite Tracking Index Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a SQLite index (`backlog/nexis.db`) that stores structured fields from `FEATURE_REQUESTS.md` and `BUGS.md`, so Claude can query project status without loading the full markdown files into context.

**Architecture:** The markdown files remain the authoritative record — all narrative content, full resolution notes, and research text stays there. The DB is a derived index of structured fields only (ID, type, title, status, severity, category, GitHub issue, resolution note, commit hash, timestamps). A Python helper (`scripts/nexis_db.py`) provides all read/write operations. `CLAUDE.md` is updated so session-start summaries, GitHub sync lookups, and status transitions all go through the DB.

**Tech Stack:** Python 3 stdlib only (`sqlite3`, `argparse`, `re`, `pathlib`). No third-party packages. SQLite WAL mode for safe concurrent reads.

---

## File Map

| File | Action | Responsibility |
|---|---|---|
| `scripts/parse_tracking.py` | **Create** | Parse `FEATURE_REQUESTS.md` / `BUGS.md` → item dicts |
| `scripts/nexis_db.py` | **Create** | CLI tool: init, summary, open, in-progress, tracked, add, start, close, sync |
| `tests/test_nexis_db.py` | **Create** | Pytest tests for parser and CLI commands |
| `backlog/nexis.db` | **Create** (via sync) | The SQLite index; committed to git |
| `.gitattributes` | **Modify** | Mark `nexis.db` as binary to prevent line-ending mangling |
| `CLAUDE.md` | **Modify** | Update session-start, GitHub sync, and status-change instructions |

---

## Task 1: Schema and DB Initialization

**Files:**
- Create: `scripts/nexis_db.py`

- [ ] **Step 1: Create `scripts/nexis_db.py` with `init_db()` and `get_db()`**

```python
#!/usr/bin/env python3
"""
nexis_db.py — SQLite index for Nexis tracking files.

Usage:
  python scripts/nexis_db.py summary
  python scripts/nexis_db.py open [--type feature|bug]
  python scripts/nexis_db.py in-progress
  python scripts/nexis_db.py tracked --issue 42
  python scripts/nexis_db.py add --id FR-133 --type feature --title "..." --category "..."
  python scripts/nexis_db.py add --id BUG-135 --type bug --title "..." --severity high --issue 22
  python scripts/nexis_db.py start --id FR-133
  python scripts/nexis_db.py close --id FR-133 --resolution "Implemented." --commit abc1234
  python scripts/nexis_db.py close --id FR-133 --declined
  python scripts/nexis_db.py sync
"""
import argparse
import sqlite3
import sys
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).parent.parent
DB_PATH = ROOT / 'backlog' / 'nexis.db'
FR_PATH = ROOT / 'FEATURE_REQUESTS.md'
BUG_PATH = ROOT / 'BUGS.md'


def now_iso() -> str:
    return datetime.now(timezone.utc).strftime('%Y-%m-%dT%H:%M:%SZ')


def get_db() -> sqlite3.Connection:
    conn = sqlite3.connect(DB_PATH)
    conn.row_factory = sqlite3.Row
    conn.execute('PRAGMA journal_mode=WAL')
    conn.execute('PRAGMA foreign_keys=ON')
    return conn


def init_db(conn: sqlite3.Connection) -> None:
    conn.executescript("""
        CREATE TABLE IF NOT EXISTS items (
            id           TEXT PRIMARY KEY,
            type         TEXT NOT NULL CHECK(type IN ('feature', 'bug')),
            title        TEXT NOT NULL,
            status       TEXT NOT NULL
                            CHECK(status IN ('open', 'in_progress', 'done', 'declined')),
            severity     TEXT CHECK(severity IN ('high', 'medium', 'low')),
            category     TEXT,
            github_issue INTEGER,
            resolution   TEXT,
            commit_hash  TEXT,
            opened_at    TEXT NOT NULL
                            DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ', 'now')),
            started_at   TEXT,
            closed_at    TEXT
        );

        CREATE INDEX IF NOT EXISTS idx_status
            ON items(status);
        CREATE INDEX IF NOT EXISTS idx_type_status
            ON items(type, status);
        CREATE INDEX IF NOT EXISTS idx_github_issue
            ON items(github_issue) WHERE github_issue IS NOT NULL;
    """)
    conn.commit()


def main():
    parser = argparse.ArgumentParser()
    sub = parser.add_subparsers(dest='cmd', required=True)
    sub.add_parser('summary')
    args = parser.parse_args()
    conn = get_db()
    init_db(conn)
    # subcommand dispatch added in later tasks
    print(f"DB initialised at {DB_PATH}")


if __name__ == '__main__':
    main()
```

- [ ] **Step 2: Verify the schema is created correctly**

```bash
cd /Users/luke/Documents/GitHub/Nexis
python scripts/nexis_db.py summary
sqlite3 backlog/nexis.db ".schema"
```

Expected output from `.schema`:
```
CREATE TABLE items (...)
CREATE INDEX idx_status ...
CREATE INDEX idx_type_status ...
CREATE INDEX idx_github_issue ...
```

- [ ] **Step 3: Commit**

```bash
git add scripts/nexis_db.py
git commit -m "feat(tracking): add nexis_db.py with SQLite schema and init"
```

---

## Task 2: Markdown Parser

**Files:**
- Create: `scripts/parse_tracking.py`
- Create: `tests/test_nexis_db.py` (parser tests only)

The parser reads both tracking files and yields item dicts. Each dict has keys: `id`, `type`, `title`, `status`, `severity`, `category`, `github_issue`, `resolution`, `commit_hash`. It does **not** set timestamp fields — those are DB concerns.

Key format rules observed in the actual files:
- FR items: `- [status] **FR-XX: Title** — body **Resolved (commit):** note`
- BUG items: `- [status] **BUG-XX: Title** (SEVERITY)` with sub-lines for description and resolution
- BUG with GH issue: `- [x] **BUG-134 / #21**: title (MEDIUM)`
- Declined: resolution note starts with "Will not implement" or "Declined"

- [ ] **Step 1: Create `scripts/parse_tracking.py`**

```python
"""
parse_tracking.py — Parse FEATURE_REQUESTS.md and BUGS.md into item dicts.

Each yielded dict has keys:
  id, type, title, status, severity, category, github_issue, resolution, commit_hash
"""
import re
from pathlib import Path
from typing import Iterator

# Matches the opening line of any item:
# - [status] **FR-XX: title** or - [x] **BUG-134 / #21**: title (MEDIUM)
_ITEM_RE = re.compile(
    r'^- \[([ x~])\] \*\*((?:FR|BUG)-\d+)(?:\s*/\s*#(\d+))?[*:\s]+(.+)'
)
_SECTION_RE = re.compile(r'^## (.+)')
_SEVERITY_INLINE_RE = re.compile(r'\((HIGH|MEDIUM|LOW)\)\s*$', re.IGNORECASE)
_SEVERITY_SECTION_RE = re.compile(r'^(HIGH|MEDIUM|LOW) Severity', re.IGNORECASE)
# Matches: **Resolved (abc1234):** note  OR  **Resolved:** (abc1234) note  OR  **Resolved:** note
_RESOLVED_RE = re.compile(
    r'\*\*Resolved[^:]*:\*\*\s*(?:\(([a-f0-9]{6,10})\)\s*)?(.+)',
    re.IGNORECASE | re.DOTALL
)
_COMMIT_TRAILING_RE = re.compile(r'\(([a-f0-9]{7,10})\)')
# GitHub issue linked in body: Issue [#2](...) or standalone #2
_ISSUE_BODY_RE = re.compile(r'Issue \[#(\d+)\]|\[#(\d+)\]')
_DECLINED_RE = re.compile(r'Will not implement|Declined:', re.IGNORECASE)
_STATUS_MAP = {' ': 'open', 'x': 'done', '~': 'in_progress'}


def _parse_file(path: Path, item_type: str) -> Iterator[dict]:
    lines = path.read_text(encoding='utf-8').splitlines()
    current_section: str | None = None
    i = 0

    while i < len(lines):
        line = lines[i]

        # Track section headers for category / severity context
        m = _SECTION_RE.match(line)
        if m:
            current_section = m.group(1).strip()
            i += 1
            continue

        # Match item start line
        m = _ITEM_RE.match(line)
        if not m:
            i += 1
            continue

        status_char, item_id, gh_inline, rest = m.groups()

        # Collect indented continuation lines into one block
        block_lines = [rest.strip()]
        i += 1
        while i < len(lines) and lines[i].startswith('  '):
            block_lines.append(lines[i].strip())
            i += 1
        block = ' '.join(block_lines)

        # ── title: text before ' — ' delimiter, stripped of severity marker
        raw_title = rest.split(' — ')[0]
        raw_title = _SEVERITY_INLINE_RE.sub('', raw_title).strip().strip('*').strip()
        # Remove trailing colon left by BUG-134 / #21 format
        title = raw_title.rstrip(':').strip()

        # ── status
        status = _STATUS_MAP[status_char]

        # ── severity: inline marker takes priority, then section header
        sev_m = _SEVERITY_INLINE_RE.search(rest)
        severity = None
        if sev_m:
            severity = sev_m.group(1).lower()
        elif current_section:
            sec_m = _SEVERITY_SECTION_RE.match(current_section)
            if sec_m:
                severity = sec_m.group(1).lower()

        # ── category
        category = current_section

        # ── GitHub issue: inline in ID line, then body link
        github_issue = int(gh_inline) if gh_inline else None
        if github_issue is None:
            ib_m = _ISSUE_BODY_RE.search(block)
            if ib_m:
                github_issue = int(ib_m.group(1) or ib_m.group(2))

        # ── resolution note and commit hash
        resolution = None
        commit_hash = None
        res_m = _RESOLVED_RE.search(block)
        if res_m:
            commit_hash = res_m.group(1)       # commit in parens right after "Resolved"
            resolution = res_m.group(2)[:300].strip()
            # Also check for trailing (abc1234) in the resolution text
            if not commit_hash:
                ch_m = _COMMIT_TRAILING_RE.search(resolution)
                if ch_m:
                    commit_hash = ch_m.group(1)

        # ── declined: done items whose resolution indicates rejection
        if status == 'done' and _DECLINED_RE.search(block):
            status = 'declined'

        yield {
            'id': item_id,
            'type': item_type,
            'title': title,
            'status': status,
            'severity': severity,
            'category': category,
            'github_issue': github_issue,
            'resolution': resolution,
            'commit_hash': commit_hash,
        }


def parse_features(path: Path) -> Iterator[dict]:
    return _parse_file(path, 'feature')


def parse_bugs(path: Path) -> Iterator[dict]:
    return _parse_file(path, 'bug')
```

- [ ] **Step 2: Write failing parser tests in `tests/test_nexis_db.py`**

```python
"""tests/test_nexis_db.py — Tests for nexis_db CLI and parse_tracking parser."""
import sys
import sqlite3
from pathlib import Path
import pytest

# Allow importing from scripts/
sys.path.insert(0, str(Path(__file__).parent.parent / 'scripts'))
from parse_tracking import parse_features, parse_bugs


# ── Parser tests ────────────────────────────────────────────────────────────

def test_parse_features_open_item(tmp_path):
    md = tmp_path / 'FEATURE_REQUESTS.md'
    md.write_text("## Community Requests\n\n- [ ] **FR-39: Plugin architecture** — Description.\n")
    items = list(parse_features(md))
    assert len(items) == 1
    assert items[0]['id'] == 'FR-39'
    assert items[0]['status'] == 'open'
    assert items[0]['title'] == 'Plugin architecture'
    assert items[0]['category'] == 'Community Requests'
    assert items[0]['github_issue'] is None
    assert items[0]['type'] == 'feature'


def test_parse_features_done_with_commit(tmp_path):
    md = tmp_path / 'FEATURE_REQUESTS.md'
    md.write_text(
        "## Performance\n\n"
        "- [x] **FR-04: Background thread cleanup** — Body. **Resolved (be20f48):** Fixed it.\n"
    )
    items = list(parse_features(md))
    assert items[0]['status'] == 'done'
    assert items[0]['commit_hash'] == 'be20f48'
    assert 'Fixed it' in items[0]['resolution']


def test_parse_features_declined(tmp_path):
    md = tmp_path / 'FEATURE_REQUESTS.md'
    md.write_text(
        "## Misc\n\n"
        "- [x] **FR-13: CLI interface** — Body. **Resolved:** Will not implement — out of scope.\n"
    )
    items = list(parse_features(md))
    assert items[0]['status'] == 'declined'


def test_parse_features_in_progress(tmp_path):
    md = tmp_path / 'FEATURE_REQUESTS.md'
    md.write_text("## Misc\n\n- [~] **FR-122: Rootkit scanner** — Removed.\n")
    items = list(parse_features(md))
    assert items[0]['status'] == 'in_progress'


def test_parse_features_github_issue_in_body(tmp_path):
    md = tmp_path / 'FEATURE_REQUESTS.md'
    md.write_text(
        "## Our Issues\n\n"
        "- [x] **FR-23: Disk analyzer** — Issue [#2](https://github.com/x). **Resolved:** Done.\n"
    )
    items = list(parse_features(md))
    assert items[0]['github_issue'] == 2


def test_parse_bugs_severity_inline(tmp_path):
    md = tmp_path / 'BUGS.md'
    md.write_text(
        "## HIGH Severity\n\n"
        "- [x] **BUG-01: Memory bug** (HIGH)\n"
        "  - **Description:** Wrong calculation.\n"
        "  - **Resolved:** Swapped assignments.\n"
    )
    items = list(parse_bugs(md))
    assert items[0]['severity'] == 'high'
    assert items[0]['id'] == 'BUG-01'
    assert items[0]['status'] == 'done'
    assert items[0]['type'] == 'bug'


def test_parse_bugs_github_issue_inline(tmp_path):
    md = tmp_path / 'BUGS.md'
    md.write_text(
        "## MEDIUM Severity\n\n"
        "- [x] **BUG-134 / #21**: [Bug] Cramped UI (MEDIUM)\n"
        "  - **Description:** Cramped layout.\n"
        "  - **Resolved:** Fixed with scrollarea.\n"
    )
    items = list(parse_bugs(md))
    assert items[0]['github_issue'] == 21
    assert items[0]['id'] == 'BUG-134'
    assert items[0]['severity'] == 'medium'


def test_parse_bugs_commit_hash_in_resolved(tmp_path):
    md = tmp_path / 'BUGS.md'
    md.write_text(
        "## MEDIUM Severity\n\n"
        "- [x] **BUG-113: Drive temperature** (MEDIUM)\n"
        "  - **Resolved:** (baee04a) Read temperature.current.\n"
    )
    items = list(parse_bugs(md))
    assert items[0]['commit_hash'] == 'baee04a'


def test_parse_multiple_sections(tmp_path):
    md = tmp_path / 'FEATURE_REQUESTS.md'
    md.write_text(
        "## Section A\n\n"
        "- [x] **FR-01: Alpha** — Done. **Resolved:** OK.\n\n"
        "## Section B\n\n"
        "- [ ] **FR-02: Beta** — Open.\n"
    )
    items = list(parse_features(md))
    assert items[0]['category'] == 'Section A'
    assert items[1]['category'] == 'Section B'
    assert items[1]['status'] == 'open'
```

- [ ] **Step 3: Run tests to confirm they fail (parser not yet imported by tests)**

```bash
cd /Users/luke/Documents/GitHub/Nexis
python -m pytest tests/test_nexis_db.py -v 2>&1 | head -30
```

Expected: multiple FAILED — `ModuleNotFoundError: No module named 'parse_tracking'` until `scripts/` is on the path, then assertion failures until the parser is saved.

- [ ] **Step 4: Run tests with the parser saved and confirm they pass**

```bash
python -m pytest tests/test_nexis_db.py -v -k "parse"
```

Expected: all `test_parse_*` tests PASSED.

- [ ] **Step 5: Commit**

```bash
git add scripts/parse_tracking.py tests/test_nexis_db.py
git commit -m "feat(tracking): add markdown parser and parser tests"
```

---

## Task 3: CLI Subcommands — Read Operations

**Files:**
- Modify: `scripts/nexis_db.py` (add `summary`, `open`, `in-progress`, `tracked`)
- Modify: `tests/test_nexis_db.py` (add DB fixture and command tests)

- [ ] **Step 1: Add DB pytest fixture and write failing tests for read commands**

Append to `tests/test_nexis_db.py`:

```python
# ── DB fixture ───────────────────────────────────────────────────────────────

from nexis_db import init_db, cmd_summary, cmd_open, cmd_in_progress, cmd_tracked


@pytest.fixture
def db(tmp_path, monkeypatch):
    db_path = tmp_path / 'nexis.db'
    import nexis_db
    monkeypatch.setattr(nexis_db, 'DB_PATH', db_path)
    conn = sqlite3.connect(db_path)
    conn.row_factory = sqlite3.Row
    init_db(conn)
    return conn


def _insert(conn, **kw):
    defaults = dict(
        id='FR-001', type='feature', title='T', status='open',
        severity=None, category=None, github_issue=None,
        opened_at='2026-01-01T00:00:00Z'
    )
    defaults.update(kw)
    conn.execute(
        "INSERT INTO items(id,type,title,status,severity,category,github_issue,opened_at) "
        "VALUES(:id,:type,:title,:status,:severity,:category,:github_issue,:opened_at)",
        defaults
    )
    conn.commit()


# ── init ─────────────────────────────────────────────────────────────────────

def test_init_creates_table(db):
    tables = db.execute(
        "SELECT name FROM sqlite_master WHERE type='table'"
    ).fetchall()
    assert any(t['name'] == 'items' for t in tables)


def test_init_creates_indexes(db):
    idxs = db.execute(
        "SELECT name FROM sqlite_master WHERE type='index'"
    ).fetchall()
    names = {r['name'] for r in idxs}
    assert {'idx_status', 'idx_type_status', 'idx_github_issue'} <= names


# ── summary ──────────────────────────────────────────────────────────────────

def test_summary_empty(db, capsys):
    cmd_summary(db, None)
    out = capsys.readouterr().out
    assert 'Features: 0 open' in out
    assert 'Bugs:' in out


def test_summary_counts(db, capsys):
    _insert(db, id='FR-001', type='feature', status='open')
    _insert(db, id='FR-002', type='feature', status='done')
    _insert(db, id='BUG-001', type='bug', status='open', severity='high')
    cmd_summary(db, None)
    out = capsys.readouterr().out
    assert 'Features: 1 open' in out
    assert 'Bugs:     1 open' in out


# ── open ─────────────────────────────────────────────────────────────────────

def test_open_lists_open_items(db, capsys):
    _insert(db, id='FR-010', type='feature', status='open', title='Alpha feature')
    _insert(db, id='FR-011', type='feature', status='done', title='Done feature')
    class A: type = None
    cmd_open(db, A())
    out = capsys.readouterr().out
    assert 'FR-010' in out
    assert 'FR-011' not in out


def test_open_filter_by_type(db, capsys):
    _insert(db, id='FR-020', type='feature', status='open', title='Feature')
    _insert(db, id='BUG-020', type='bug', status='open', title='Bug', severity='low')
    class A: type = 'bug'
    cmd_open(db, A())
    out = capsys.readouterr().out
    assert 'BUG-020' in out
    assert 'FR-020' not in out


# ── in-progress ───────────────────────────────────────────────────────────────

def test_in_progress_shows_started(db, capsys):
    _insert(db, id='FR-030', type='feature', status='in_progress', title='WIP')
    db.execute(
        "UPDATE items SET started_at='2026-04-01T10:00:00Z' WHERE id='FR-030'"
    )
    db.commit()
    cmd_in_progress(db, None)
    out = capsys.readouterr().out
    assert 'FR-030' in out
    assert '2026-04-01' in out


# ── tracked ───────────────────────────────────────────────────────────────────

def test_tracked_found(db, capsys):
    _insert(db, id='FR-040', type='feature', status='open', github_issue=55)
    class A: issue = 55
    cmd_tracked(db, A())
    assert capsys.readouterr().out.strip() == 'FR-040'


def test_tracked_not_found(db, capsys):
    class A: issue = 999
    cmd_tracked(db, A())
    assert capsys.readouterr().out.strip() == ''
```

- [ ] **Step 2: Run tests to confirm they fail**

```bash
python -m pytest tests/test_nexis_db.py -v -k "summary or open or in_progress or tracked or init" 2>&1 | tail -20
```

Expected: FAILED — `ImportError` for `cmd_summary` etc. since those aren't in `nexis_db.py` yet.

- [ ] **Step 3: Add read commands to `nexis_db.py`**

Replace the stub `main()` and add these functions before it:

```python
def cmd_summary(conn: sqlite3.Connection, _args) -> None:
    rows = conn.execute(
        "SELECT type, status, COUNT(*) AS n FROM items GROUP BY type, status"
    ).fetchall()
    counts = {(r['type'], r['status']): r['n'] for r in rows}

    def c(t, s):
        return counts.get((t, s), 0)

    print(
        f"Features: {c('feature','open')} open, {c('feature','in_progress')} in-progress, "
        f"{c('feature','done')} done, {c('feature','declined')} declined"
    )
    print(
        f"Bugs:     {c('bug','open')} open, {c('bug','in_progress')} in-progress, "
        f"{c('bug','done')} done"
    )


def cmd_open(conn: sqlite3.Connection, args) -> None:
    sql = "SELECT id, type, severity, title FROM items WHERE status='open'"
    params: list = []
    if args.type:
        sql += " AND type=?"
        params.append(args.type)
    sql += " ORDER BY type, id"
    rows = conn.execute(sql, params).fetchall()
    if not rows:
        print("No open items.")
        return
    for r in rows:
        sev = f" [{r['severity'].upper()}]" if r['severity'] else ''
        print(f"  {r['id']}{sev}: {r['title']}")


def cmd_in_progress(conn: sqlite3.Connection, _args) -> None:
    rows = conn.execute(
        "SELECT id, type, title, started_at FROM items "
        "WHERE status='in_progress' ORDER BY started_at"
    ).fetchall()
    if not rows:
        print("No in-progress items.")
        return
    for r in rows:
        print(f"  {r['id']} ({r['type']}): {r['title']}  [started {r['started_at'] or '?'}]")


def cmd_tracked(conn: sqlite3.Connection, args) -> None:
    row = conn.execute(
        "SELECT id FROM items WHERE github_issue=?", (args.issue,)
    ).fetchone()
    print(row['id'] if row else '')
```

Also replace the stub `main()`:

```python
def main():
    parser = argparse.ArgumentParser()
    sub = parser.add_subparsers(dest='cmd', required=True)

    sub.add_parser('summary')

    p = sub.add_parser('open')
    p.add_argument('--type', choices=['feature', 'bug'])

    sub.add_parser('in-progress')

    p = sub.add_parser('tracked')
    p.add_argument('--issue', type=int, required=True)

    args = parser.parse_args()
    conn = get_db()
    init_db(conn)
    dispatch = {
        'summary': cmd_summary,
        'open': cmd_open,
        'in-progress': cmd_in_progress,
        'tracked': cmd_tracked,
    }
    dispatch[args.cmd](conn, args)


if __name__ == '__main__':
    main()
```

- [ ] **Step 4: Run tests — confirm read command tests pass**

```bash
python -m pytest tests/test_nexis_db.py -v -k "summary or open or in_progress or tracked or init"
```

Expected: all PASSED.

- [ ] **Step 5: Commit**

```bash
git add scripts/nexis_db.py tests/test_nexis_db.py
git commit -m "feat(tracking): add summary, open, in-progress, tracked commands"
```

---

## Task 4: CLI Subcommands — Write Operations

**Files:**
- Modify: `scripts/nexis_db.py` (add `add`, `start`, `close`)
- Modify: `tests/test_nexis_db.py` (add write command tests)

- [ ] **Step 1: Write failing tests for write commands**

Append to `tests/test_nexis_db.py`:

```python
from nexis_db import cmd_add, cmd_start, cmd_close


# ── add ───────────────────────────────────────────────────────────────────────

def test_add_feature(db, capsys):
    class A:
        id='FR-200'; type='feature'; title='Test feature'
        category='Testing'; severity=None; issue=None
    cmd_add(db, A())
    row = db.execute("SELECT * FROM items WHERE id='FR-200'").fetchone()
    assert row['status'] == 'open'
    assert row['type'] == 'feature'
    assert row['title'] == 'Test feature'
    assert row['opened_at'] is not None


def test_add_bug_with_severity_and_issue(db, capsys):
    class A:
        id='BUG-200'; type='bug'; title='Test bug'
        category='MEDIUM Severity'; severity='medium'; issue=42
    cmd_add(db, A())
    row = db.execute("SELECT * FROM items WHERE id='BUG-200'").fetchone()
    assert row['severity'] == 'medium'
    assert row['github_issue'] == 42


# ── start ─────────────────────────────────────────────────────────────────────

def test_start_sets_in_progress(db):
    _insert(db, id='FR-201', type='feature', status='open')
    class A: id = 'FR-201'
    cmd_start(db, A())
    row = db.execute(
        "SELECT status, started_at FROM items WHERE id='FR-201'"
    ).fetchone()
    assert row['status'] == 'in_progress'
    assert row['started_at'] is not None


def test_start_nonexistent_exits(db):
    class A: id = 'FR-999'
    with pytest.raises(SystemExit):
        cmd_start(db, A())


# ── close ─────────────────────────────────────────────────────────────────────

def test_close_done(db):
    _insert(db, id='FR-202', type='feature', status='in_progress')
    class A:
        id='FR-202'; resolution='Implemented it'; commit='abc1234'; declined=False
    cmd_close(db, A())
    row = db.execute(
        "SELECT status, resolution, commit_hash, closed_at FROM items WHERE id='FR-202'"
    ).fetchone()
    assert row['status'] == 'done'
    assert row['resolution'] == 'Implemented it'
    assert row['commit_hash'] == 'abc1234'
    assert row['closed_at'] is not None


def test_close_declined(db):
    _insert(db, id='FR-203', type='feature', status='open')
    class A:
        id='FR-203'; resolution='Out of scope'; commit=None; declined=True
    cmd_close(db, A())
    row = db.execute("SELECT status FROM items WHERE id='FR-203'").fetchone()
    assert row['status'] == 'declined'


def test_close_nonexistent_exits(db):
    class A:
        id='FR-998'; resolution=None; commit=None; declined=False
    with pytest.raises(SystemExit):
        cmd_close(db, A())
```

- [ ] **Step 2: Run failing tests to confirm they fail**

```bash
python -m pytest tests/test_nexis_db.py -v -k "add or start or close" 2>&1 | tail -15
```

Expected: FAILED — `ImportError` for `cmd_add` etc.

- [ ] **Step 3: Add write commands to `nexis_db.py`**

Add these functions before `main()`:

```python
def cmd_add(conn: sqlite3.Connection, args) -> None:
    conn.execute(
        "INSERT INTO items "
        "(id, type, title, status, severity, category, github_issue, opened_at) "
        "VALUES (?, ?, ?, 'open', ?, ?, ?, ?)",
        (args.id, args.type, args.title,
         args.severity, args.category, args.issue, now_iso())
    )
    conn.commit()
    print(f"Added {args.id}.")


def cmd_start(conn: sqlite3.Connection, args) -> None:
    conn.execute(
        "UPDATE items SET status='in_progress', started_at=? WHERE id=?",
        (now_iso(), args.id)
    )
    if conn.execute("SELECT changes()").fetchone()[0] == 0:
        print(f"Error: {args.id} not found.", file=sys.stderr)
        sys.exit(1)
    conn.commit()
    print(f"Started {args.id}.")


def cmd_close(conn: sqlite3.Connection, args) -> None:
    status = 'declined' if args.declined else 'done'
    conn.execute(
        "UPDATE items SET status=?, resolution=?, commit_hash=?, closed_at=? WHERE id=?",
        (status, args.resolution, args.commit, now_iso(), args.id)
    )
    if conn.execute("SELECT changes()").fetchone()[0] == 0:
        print(f"Error: {args.id} not found.", file=sys.stderr)
        sys.exit(1)
    conn.commit()
    print(f"Closed {args.id} as {status}.")
```

Add the write subcommands to `main()` (replace the existing `main()` entirely):

```python
def main():
    parser = argparse.ArgumentParser()
    sub = parser.add_subparsers(dest='cmd', required=True)

    sub.add_parser('summary')

    p = sub.add_parser('open')
    p.add_argument('--type', choices=['feature', 'bug'])

    sub.add_parser('in-progress')

    p = sub.add_parser('tracked')
    p.add_argument('--issue', type=int, required=True)

    p = sub.add_parser('add')
    p.add_argument('--id', required=True)
    p.add_argument('--type', required=True, choices=['feature', 'bug'])
    p.add_argument('--title', required=True)
    p.add_argument('--category', default=None)
    p.add_argument('--severity', choices=['high', 'medium', 'low'], default=None)
    p.add_argument('--issue', type=int, default=None)

    p = sub.add_parser('start')
    p.add_argument('--id', required=True)

    p = sub.add_parser('close')
    p.add_argument('--id', required=True)
    p.add_argument('--resolution', default=None)
    p.add_argument('--commit', default=None)
    p.add_argument('--declined', action='store_true')

    sub.add_parser('sync')

    args = parser.parse_args()
    conn = get_db()
    init_db(conn)
    dispatch = {
        'summary':     cmd_summary,
        'open':        cmd_open,
        'in-progress': cmd_in_progress,
        'tracked':     cmd_tracked,
        'add':         cmd_add,
        'start':       cmd_start,
        'close':       cmd_close,
        'sync':        cmd_sync,   # defined in Task 5
    }
    dispatch[args.cmd](conn, args)


if __name__ == '__main__':
    main()
```

Note: `cmd_sync` will be defined in Task 5. If you run `main()` before Task 5, the `sync` dispatch entry will raise a `NameError`. That's fine — the tests for Tasks 3 and 4 call the command functions directly and don't invoke `main()`.

- [ ] **Step 4: Run all tests so far**

```bash
python -m pytest tests/test_nexis_db.py -v
```

Expected: all tests PASSED (parser tests + read + write). The `sync` integration test added in Task 5 doesn't exist yet.

- [ ] **Step 5: Commit**

```bash
git add scripts/nexis_db.py tests/test_nexis_db.py
git commit -m "feat(tracking): add add, start, close commands"
```

---

## Task 5: Sync Command and Migration

**Files:**
- Modify: `scripts/nexis_db.py` (add `cmd_sync`)
- Modify: `tests/test_nexis_db.py` (add sync integration test)
- Create: `backlog/nexis.db` (produced by running sync)

- [ ] **Step 1: Write the failing sync integration test**

Append to `tests/test_nexis_db.py`:

```python
from nexis_db import cmd_sync


def test_sync_populates_from_real_files(db, monkeypatch):
    """Run sync against the real tracking files and verify row counts match grep."""
    import nexis_db
    monkeypatch.setattr(nexis_db, 'FR_PATH', Path(__file__).parent.parent / 'FEATURE_REQUESTS.md')
    monkeypatch.setattr(nexis_db, 'BUG_PATH', Path(__file__).parent.parent / 'BUGS.md')
    cmd_sync(db, None)
    total = db.execute("SELECT COUNT(*) FROM items").fetchone()[0]
    assert total > 200  # 136 bugs + 137+ FRs
    # Verify known open count matches what grep reports
    open_count = db.execute(
        "SELECT COUNT(*) FROM items WHERE status='open'"
    ).fetchone()[0]
    assert open_count == 4  # 4 open FRs, 0 open bugs (confirmed by grep)
    # Verify no bugs are open
    open_bugs = db.execute(
        "SELECT COUNT(*) FROM items WHERE type='bug' AND status='open'"
    ).fetchone()[0]
    assert open_bugs == 0
    # Verify BUG-134 linked to GitHub issue #21
    bug134 = db.execute(
        "SELECT github_issue FROM items WHERE id='BUG-134'"
    ).fetchone()
    assert bug134 is not None
    assert bug134['github_issue'] == 21
```

- [ ] **Step 2: Run test to confirm it fails**

```bash
python -m pytest tests/test_nexis_db.py::test_sync_populates_from_real_files -v 2>&1 | tail -10
```

Expected: FAILED — `ImportError: cannot import name 'cmd_sync'`.

- [ ] **Step 3: Add `cmd_sync` to `nexis_db.py`**

Add this function before `main()`:

```python
def cmd_sync(conn: sqlite3.Connection, _args) -> None:
    """Re-parse both markdown files and INSERT OR REPLACE all items."""
    # Import here to avoid circular dependency in tests that mock FR_PATH/BUG_PATH
    from parse_tracking import parse_features, parse_bugs  # noqa: PLC0415

    inserted = updated = 0
    all_items = [*parse_features(FR_PATH), *parse_bugs(BUG_PATH)]

    for item in all_items:
        existing = conn.execute(
            "SELECT id FROM items WHERE id=?", (item['id'],)
        ).fetchone()
        if existing is None:
            conn.execute(
                "INSERT INTO items "
                "(id, type, title, status, severity, category, "
                " github_issue, resolution, commit_hash) "
                "VALUES (:id, :type, :title, :status, :severity, :category, "
                "        :github_issue, :resolution, :commit_hash)",
                item
            )
            inserted += 1
        else:
            conn.execute(
                "UPDATE items SET "
                "  title=:title, status=:status, severity=:severity, "
                "  category=:category, github_issue=:github_issue, "
                "  resolution=:resolution, commit_hash=:commit_hash "
                "WHERE id=:id",
                item
            )
            updated += 1

    conn.commit()
    print(f"Sync complete: {inserted} inserted, {updated} updated.")
```

- [ ] **Step 4: Run the sync integration test**

```bash
python -m pytest tests/test_nexis_db.py::test_sync_populates_from_real_files -v
```

Expected: PASSED.

- [ ] **Step 5: Run the full test suite**

```bash
python -m pytest tests/test_nexis_db.py -v
```

Expected: all tests PASSED.

- [ ] **Step 6: Run the migration against the real files to create `backlog/nexis.db`**

```bash
cd /Users/luke/Documents/GitHub/Nexis
python scripts/nexis_db.py sync
python scripts/nexis_db.py summary
python scripts/nexis_db.py open
python scripts/nexis_db.py in-progress
```

Expected output from `summary`:
```
Features: 4 open, 1 in-progress, 130+ done, 2+ declined
Bugs:     0 open, 0 in-progress, 136 done
```

Verify `open` lists the 4 known open FRs (FR-39, FR-40, FR-67, FR-91).  
Verify `in-progress` lists FR-122.

- [ ] **Step 7: Mark `nexis.db` as binary in `.gitattributes`**

```bash
echo 'backlog/nexis.db binary' >> .gitattributes
```

- [ ] **Step 8: Commit everything**

```bash
git add scripts/nexis_db.py tests/test_nexis_db.py backlog/nexis.db .gitattributes
git commit -m "feat(tracking): add sync command and run initial migration (nexis.db)"
```

---

## Task 6: Update CLAUDE.md Workflow

**Files:**
- Modify: `CLAUDE.md`

Four specific sections need updating. Each change is described with the exact old text and replacement.

- [ ] **Step 1: Update session-start tracking file reads**

Find the "Read tracking files" step under `## Feature / Bug Resolution Workflow` → Session Start instructions. Replace:

```markdown
2. **Read tracking files:**
   - Read `FEATURE_REQUESTS.md` — count open (`[ ]`), in-progress (`[~]`), and completed (`[x]`) items
   - Read `BUGS.md` — count open, in-progress, and completed items by severity
```

With:

```markdown
2. **Read tracking files:**
   - Run `python scripts/nexis_db.py summary` to get counts by type and status.
   - If `backlog/nexis.db` does not exist yet, run `python scripts/nexis_db.py sync` first to build it from the markdown files.
```

- [ ] **Step 2: Update GitHub Issues sync — issue lookup**

Find step 2 of the GitHub Issues Sync section:

```markdown
2. **Identify untracked issues** — An issue is *untracked* if no line in `FEATURE_REQUESTS.md` or `BUGS.md` references its GitHub issue number (e.g., `#42`). Search both files for each issue number.
```

Replace with:

```markdown
2. **Identify untracked issues** — For each open issue, run:
   ```bash
   python scripts/nexis_db.py tracked --issue <number>
   ```
   If the output is empty, the issue is untracked and needs to be added.
```

- [ ] **Step 3: Add DB write step to "new item" workflow**

Find step 4 in the GitHub Issues Sync section:

```markdown
4. **Add to the appropriate tracking file:**
   - Use the next sequential ID (`BUG-XX` or `FR-XX`)
   - Format: `- [ ] **BUG-XX / #<issue>**: <issue title>` (include the GitHub `#number` so future syncs skip it)
   - For bugs, assign severity based on labels or title keywords: `crash`/`data loss` → HIGH, `incorrect behavior` → MEDIUM, cosmetic/minor → LOW
   - Place under the correct severity section (BUGS.md) or category section (FEATURE_REQUESTS.md); use "Uncategorized" if unclear
```

Replace with:

```markdown
4. **Add to the appropriate tracking file and index:**
   - Use the next sequential ID (`BUG-XX` or `FR-XX`)
   - Append to the markdown file: `- [ ] **BUG-XX / #<issue>**: <issue title>` under the correct section
   - Then add to the DB index:
     ```bash
     # For a bug:
     python scripts/nexis_db.py add --id BUG-XX --type bug --title "<title>" \
       --severity <high|medium|low> --issue <number> --category "<section>"
     # For a feature:
     python scripts/nexis_db.py add --id FR-XX --type feature --title "<title>" \
       --issue <number> --category "Uncategorized"
     ```
```

- [ ] **Step 4: Add DB update steps to Phase 3 implementation instructions**

Find in Phase 3:

```markdown
### Phase 3 — Implementation

1. Implement fully once approved. Do not stop until all tasks are completed.
2. Mark each task `[x]` in the plan as completed.
```

Replace with:

```markdown
### Phase 3 — Implementation

1. Implement fully once approved. Do not stop until all tasks are completed.
2. Mark each task `[x]` in the plan as completed.
3. When starting work on a tracked item, update the DB:
   ```bash
   python scripts/nexis_db.py start --id <ID>
   ```
   Also change `[ ]` → `[~]` in the markdown tracking file.
4. When closing a tracked item, update the DB:
   ```bash
   python scripts/nexis_db.py close --id <ID> --resolution "<brief note>" --commit <hash>
   # Or for declined items:
   python scripts/nexis_db.py close --id <ID> --declined --resolution "Will not implement — reason"
   ```
   Also change `[~]` → `[x]` in the markdown tracking file and add the `**Resolved:**` note.
```

- [ ] **Step 5: Add a SQLite Index section to project structure documentation**

Find the `## Tracking Files` section in `CLAUDE.md` and append:

```markdown
## SQLite Index

`backlog/nexis.db` is a derived SQLite index of the structured fields in `FEATURE_REQUESTS.md` and `BUGS.md`. Use `scripts/nexis_db.py` for all queries and status updates — never write to the DB with raw SQL outside of that script.

The markdown files remain authoritative for full content. If the DB drifts out of sync (e.g., after a manual markdown edit), run:
```bash
python scripts/nexis_db.py sync
```
This re-parses both files and reconciles the DB. It is safe to run at any time.

Common queries:
```bash
python scripts/nexis_db.py summary          # session-start counts
python scripts/nexis_db.py open             # all open items
python scripts/nexis_db.py open --type bug  # open bugs only
python scripts/nexis_db.py in-progress      # active work
python scripts/nexis_db.py tracked --issue 42  # check if GH issue is tracked
```
```

- [ ] **Step 6: Commit**

```bash
git add CLAUDE.md
git commit -m "docs(claude): update workflow to use SQLite index for queries and status updates"
```

---

## Self-Review

### Spec Coverage

| Requirement | Task |
|---|---|
| SQLite DB at `backlog/nexis.db` | Task 1 (schema), Task 5 (migration) |
| Python helper script | Tasks 1–5 (`nexis_db.py`) |
| Parse both markdown files | Task 2 (`parse_tracking.py`) |
| `summary` command (session-start) | Task 3 |
| `open` / `in-progress` commands | Task 3 |
| `tracked --issue N` (GH sync) | Task 3 |
| `add` command (new items) | Task 4 |
| `start` / `close` commands (status transitions) | Task 4 |
| `sync` command (drift repair + migration) | Task 5 |
| `.gitattributes` binary marker | Task 5 |
| CLAUDE.md workflow updated | Task 6 |
| Tests for all commands | Tasks 2, 3, 4, 5 |

No gaps found.

### Placeholder Scan

No "TBD", "TODO", or vague steps present. All code blocks are complete. All test assertions specify exact expected values. Command outputs are specified.

### Type Consistency

- `cmd_*` functions all take `(conn: sqlite3.Connection, args)` — consistent across Tasks 3, 4, 5.
- `_insert()` test helper uses same column names throughout Tasks 3, 4, 5.
- `parse_features` / `parse_bugs` return dicts with the same 9 keys used in `cmd_sync`'s INSERT/UPDATE.
- `DB_PATH`, `FR_PATH`, `BUG_PATH` monkeypatched consistently in all integration tests.

---
