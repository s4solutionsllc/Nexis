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


def _sync_stub(_conn, _args):
    import sys
    print("Error: sync not yet implemented.", file=sys.stderr)
    sys.exit(1)


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
        'sync':        _sync_stub,
    }
    dispatch[args.cmd](conn, args)


if __name__ == '__main__':
    main()
