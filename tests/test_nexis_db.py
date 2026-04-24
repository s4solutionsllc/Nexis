"""tests/test_nexis_db.py — Tests for nexis_db CLI and parse_tracking parser."""
import sys
import sqlite3
from pathlib import Path
import pytest

# Allow importing from scripts/
sys.path.insert(0, str(Path(__file__).parent.parent / 'scripts'))
from parse_tracking import parse_features, parse_bugs
from nexis_db import init_db, cmd_summary, cmd_open, cmd_in_progress, cmd_tracked
from nexis_db import cmd_add, cmd_start, cmd_close


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
    assert items[0]['title'] == 'Cramped UI'


def test_parse_bugs_commit_hash_in_resolved(tmp_path):
    md = tmp_path / 'BUGS.md'
    md.write_text(
        "## MEDIUM Severity\n\n"
        "- [x] **BUG-113: Drive temperature** (MEDIUM)\n"
        "  - **Resolved:** (baee04a) Read temperature.current.\n"
    )
    items = list(parse_bugs(md))
    assert items[0]['commit_hash'] == 'baee04a'


def test_parse_bugs_bare_commit_hash(tmp_path):
    md = tmp_path / 'BUGS.md'
    md.write_text(
        "## MEDIUM Severity\n\n"
        "- [x] **BUG-104: GPU bug** (MEDIUM)\n"
        "  - **Resolved:** 29665af, b0c3b1d Fixed it.\n"
    )
    items = list(parse_bugs(md))
    assert items[0]['commit_hash'] == '29665af'


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


# ── DB fixture ───────────────────────────────────────────────────────────────

@pytest.fixture
def db(tmp_path, monkeypatch):
    db_path = tmp_path / 'nexis.db'
    import nexis_db
    monkeypatch.setattr(nexis_db, 'DB_PATH', db_path)
    conn = nexis_db.get_db()
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


# ── add ───────────────────────────────────────────────────────────────────────

def test_add_feature(db, capsys):
    class A:
        id = 'FR-200'; type = 'feature'; title = 'Test feature'
        category = 'Testing'; severity = None; issue = None
    cmd_add(db, A())
    row = db.execute("SELECT * FROM items WHERE id='FR-200'").fetchone()
    assert row['status'] == 'open'
    assert row['type'] == 'feature'
    assert row['title'] == 'Test feature'
    assert row['opened_at'] is not None


def test_add_bug_with_severity_and_issue(db, capsys):
    class A:
        id = 'BUG-200'; type = 'bug'; title = 'Test bug'
        category = 'MEDIUM Severity'; severity = 'medium'; issue = 42
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
    with pytest.raises(SystemExit) as exc_info:
        cmd_start(db, A())
    assert exc_info.value.code == 1


# ── close ─────────────────────────────────────────────────────────────────────

def test_close_done(db):
    _insert(db, id='FR-202', type='feature', status='in_progress')
    class A:
        id = 'FR-202'; resolution = 'Implemented it'; commit = 'abc1234'; declined = False
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
        id = 'FR-203'; resolution = 'Out of scope'; commit = None; declined = True
    cmd_close(db, A())
    row = db.execute("SELECT status FROM items WHERE id='FR-203'").fetchone()
    assert row['status'] == 'declined'


def test_close_nonexistent_exits(db):
    class A:
        id = 'FR-998'; resolution = None; commit = None; declined = False
    with pytest.raises(SystemExit) as exc_info:
        cmd_close(db, A())
    assert exc_info.value.code == 1


def test_add_duplicate_exits(db):
    _insert(db, id='FR-300', type='feature', status='open')
    class A:
        id = 'FR-300'; type = 'feature'; title = 'Dup'; category = None; severity = None; issue = None
    with pytest.raises(SystemExit) as exc_info:
        cmd_add(db, A())
    assert exc_info.value.code == 1


def test_start_already_closed_exits(db):
    _insert(db, id='FR-301', type='feature', status='done')
    class A: id = 'FR-301'
    with pytest.raises(SystemExit) as exc_info:
        cmd_start(db, A())
    assert exc_info.value.code == 1


# ── sync integration ──────────────────────────────────────────────────────────

from nexis_db import cmd_sync


def test_sync_populates_from_real_files(db, monkeypatch):
    """Run sync against the real tracking files and verify row counts match grep."""
    import nexis_db
    monkeypatch.setattr(nexis_db, 'FR_PATH', Path(__file__).parent.parent / 'FEATURE_REQUESTS.md')
    monkeypatch.setattr(nexis_db, 'BUG_PATH', Path(__file__).parent.parent / 'BUGS.md')
    cmd_sync(db, None)
    total = db.execute("SELECT COUNT(*) FROM items").fetchone()[0]
    assert total > 260  # real files have 267 rows; this catches >3% loss
    open_count = db.execute(
        "SELECT COUNT(*) FROM items WHERE status='open'"
    ).fetchone()[0]
    assert open_count >= 3  # known: FR-39, FR-40, FR-67 are open
    bug134 = db.execute(
        "SELECT github_issue FROM items WHERE id='BUG-134'"
    ).fetchone()
    assert bug134 is not None
    assert bug134['github_issue'] == 21
    # Verify a known feature item
    fr04 = db.execute("SELECT status FROM items WHERE id='FR-04'").fetchone()
    assert fr04 is not None
    assert fr04['status'] == 'done'


def test_sync_is_idempotent(db, monkeypatch):
    """Running sync twice should produce the same total row count."""
    import nexis_db
    monkeypatch.setattr(nexis_db, 'FR_PATH', Path(__file__).parent.parent / 'FEATURE_REQUESTS.md')
    monkeypatch.setattr(nexis_db, 'BUG_PATH', Path(__file__).parent.parent / 'BUGS.md')
    cmd_sync(db, None)
    total_first = db.execute("SELECT COUNT(*) FROM items").fetchone()[0]
    cmd_sync(db, None)
    total_second = db.execute("SELECT COUNT(*) FROM items").fetchone()[0]
    assert total_first == total_second


def test_sync_preserves_lifecycle_state(db, monkeypatch):
    """A row set in_progress by cmd_start should remain in_progress after re-sync."""
    import nexis_db
    monkeypatch.setattr(nexis_db, 'FR_PATH', Path(__file__).parent.parent / 'FEATURE_REQUESTS.md')
    monkeypatch.setattr(nexis_db, 'BUG_PATH', Path(__file__).parent.parent / 'BUGS.md')
    # First sync: populate from real files
    cmd_sync(db, None)
    # Manually set a known open item to in_progress (simulating cmd_start)
    # FR-39 is known to be open in the real markdown
    db.execute(
        "UPDATE items SET status='in_progress', started_at='2026-04-24T00:00:00Z' WHERE id='FR-39'"
    )
    db.commit()
    # Re-sync: markdown still shows FR-39 as open, but DB should preserve in_progress
    cmd_sync(db, None)
    row = db.execute("SELECT status, started_at FROM items WHERE id='FR-39'").fetchone()
    assert row['status'] == 'in_progress'
    assert row['started_at'] == '2026-04-24T00:00:00Z'
