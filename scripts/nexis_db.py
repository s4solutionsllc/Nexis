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
    DB_PATH.parent.mkdir(parents=True, exist_ok=True)
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
    print(f"DB initialised at {DB_PATH}")


if __name__ == '__main__':
    main()
