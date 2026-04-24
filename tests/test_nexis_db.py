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
