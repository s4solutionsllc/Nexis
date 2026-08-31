#!/usr/bin/env python3
"""Assemble per-change changelog fragments into CHANGELOG.md at release cut.

Every PR drops one file under `changelog.d/` instead of editing the shared
`## [Unreleased]` block in CHANGELOG.md. Distinct filenames never textually
conflict, so concurrent PRs stop invalidating each other (SSO-23951).

Fragment naming:  changelog.d/<slug>.<type>.md
  <slug>  free-form, conventionally the issue id + a short description
          e.g. sso-23853-menubar-health-score
  <type>  one of: added, changed, deprecated, removed, fixed, security

The file body is the changelog entry itself — plain markdown, no leading "- ".
It may span multiple lines; it is emitted as a single bullet.

Usage:
  changelog_fragments.py lint                     # validate every fragment
  changelog_fragments.py render                   # print assembled sections
  changelog_fragments.py apply --version 2.10.0 --date 2026-08-31
                                                  # fold into CHANGELOG.md,
                                                  # delete the fragments
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
FRAGMENT_DIR = REPO_ROOT / "changelog.d"
CHANGELOG = REPO_ROOT / "CHANGELOG.md"

# Keep a Changelog section order. Rendering always follows this order so the
# assembled output is deterministic regardless of filesystem iteration order.
SECTIONS = ["Added", "Changed", "Deprecated", "Removed", "Fixed", "Security"]
TYPES = {s.lower(): s for s in SECTIONS}

SLUG_RE = re.compile(r"^[a-z0-9][a-z0-9._-]*$", re.IGNORECASE)


class FragmentError(Exception):
    pass


def parse_fragment(path: Path) -> tuple[str, str]:
    """Return (section, body) for one fragment file, or raise FragmentError."""
    name = path.name
    if not name.endswith(".md"):
        raise FragmentError(f"{name}: fragments must end in .md")
    stem = name[: -len(".md")]
    slug, _, kind = stem.rpartition(".")
    if not slug:
        raise FragmentError(
            f"{name}: expected <slug>.<type>.md, e.g. sso-1234-short-title.fixed.md"
        )
    if kind.lower() not in TYPES:
        raise FragmentError(
            f"{name}: unknown type '{kind}' — expected one of {', '.join(TYPES)}"
        )
    if not SLUG_RE.match(slug):
        raise FragmentError(
            f"{name}: slug '{slug}' must be alphanumeric with - _ . separators"
        )
    body = path.read_text(encoding="utf-8").strip()
    if not body:
        raise FragmentError(f"{name}: fragment is empty")
    if body.lstrip().startswith("- "):
        raise FragmentError(
            f"{name}: drop the leading '- ' — the bullet marker is added on assembly"
        )
    return TYPES[kind.lower()], body


def load_fragments() -> dict[str, list[str]]:
    """Collect fragments grouped by section, sorted by filename within a section."""
    if not FRAGMENT_DIR.is_dir():
        return {}
    errors: list[str] = []
    grouped: dict[str, list[str]] = {}
    for path in sorted(FRAGMENT_DIR.iterdir()):
        if path.name.startswith(".") or path.name == "README.md" or path.is_dir():
            continue
        try:
            section, body = parse_fragment(path)
        except FragmentError as exc:
            errors.append(str(exc))
            continue
        grouped.setdefault(section, []).append(body)
    if errors:
        raise FragmentError("\n".join(errors))
    return grouped


def as_bullet(body: str) -> str:
    """Render one fragment body as a markdown list item, indenting continuations."""
    lines = body.splitlines()
    out = ["- " + lines[0].rstrip()]
    for line in lines[1:]:
        out.append(("  " + line.rstrip()).rstrip())
    return "\n".join(out)


def render(grouped: dict[str, list[str]]) -> str:
    blocks = []
    for section in SECTIONS:
        if section not in grouped:
            continue
        bullets = "\n".join(as_bullet(b) for b in grouped[section])
        blocks.append(f"### {section}\n{bullets}")
    return "\n\n".join(blocks)


# --- CHANGELOG.md surgery -------------------------------------------------


def split_unreleased(text: str) -> tuple[str, str, str]:
    """Split CHANGELOG.md into (head, unreleased_body, tail).

    `head` ends just before the `## [Unreleased]` header; `tail` starts at the
    next `## ` header. Raises if the Unreleased block is missing.
    """
    m = re.search(r"^## \[Unreleased\][^\n]*\n", text, re.MULTILINE)
    if not m:
        raise FragmentError("CHANGELOG.md has no '## [Unreleased]' section")
    body_start = m.end()
    nxt = re.search(r"^## ", text[body_start:], re.MULTILINE)
    body_end = body_start + (nxt.start() if nxt else len(text) - body_start)
    return text[: m.start()], text[body_start:body_end], text[body_end:]


def parse_sections(body: str) -> dict[str, list[str]]:
    """Parse an existing '### Section' block into section -> list of raw bullets.

    Bullets keep their original text (without the '- ' marker) so they can be
    re-emitted alongside freshly assembled fragments.
    """
    grouped: dict[str, list[str]] = {}
    current: str | None = None
    buf: list[str] = []

    def flush() -> None:
        if current is None:
            return
        text = "\n".join(buf).strip("\n")
        if not text.strip():
            return
        # Split on top-level "- " bullets, preserving indented continuations.
        items: list[str] = []
        for line in text.splitlines():
            if line.startswith("- "):
                items.append(line[2:])
            elif items:
                items[-1] += "\n" + line[2:] if line.startswith("  ") else "\n" + line
            elif line.strip():
                items.append(line)
        grouped.setdefault(current, []).extend(i.strip("\n") for i in items if i.strip())

    for line in body.splitlines():
        h = re.match(r"^### (.+?)\s*$", line)
        if h:
            flush()
            buf = []
            current = h.group(1).strip()
            continue
        buf.append(line)
    flush()
    return grouped


def apply(version: str, date: str, keep: bool = False) -> None:
    fragments = load_fragments()
    text = CHANGELOG.read_text(encoding="utf-8")
    head, unreleased_body, tail = split_unreleased(text)

    # Merge hand-written Unreleased content (pre-fragment entries, and the
    # release-cut edits a human may still want to make) with the fragments.
    merged = parse_sections(unreleased_body)
    for section, bodies in fragments.items():
        merged.setdefault(section, []).extend(bodies)

    unknown = [s for s in merged if s not in SECTIONS]
    if unknown:
        raise FragmentError(
            "CHANGELOG.md [Unreleased] has non-standard section(s): "
            + ", ".join(sorted(unknown))
        )

    rendered = render(merged)
    if not rendered:
        raise FragmentError(
            "nothing to release — no changelog fragments and an empty [Unreleased]"
        )

    new = (
        f"{head}## [Unreleased]\n\n## [{version}] - {date}\n\n{rendered}\n\n{tail.lstrip(chr(10))}"
    )
    CHANGELOG.write_text(new, encoding="utf-8")
    print(f"CHANGELOG.md: cut [{version}] - {date}")

    if keep:
        return
    removed = 0
    for path in sorted(FRAGMENT_DIR.iterdir()) if FRAGMENT_DIR.is_dir() else []:
        if path.name.startswith(".") or path.name == "README.md" or path.is_dir():
            continue
        path.unlink()
        removed += 1
    print(f"changelog.d/: consumed {removed} fragment(s)")


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)
    sub.add_parser("lint", help="validate every fragment filename and body")
    sub.add_parser("render", help="print the assembled changelog sections")
    ap_apply = sub.add_parser("apply", help="fold fragments into CHANGELOG.md")
    ap_apply.add_argument("--version", required=True)
    ap_apply.add_argument("--date", required=True)
    ap_apply.add_argument(
        "--keep", action="store_true", help="do not delete fragments after folding"
    )
    args = ap.parse_args()

    try:
        if args.cmd == "lint":
            grouped = load_fragments()
            n = sum(len(v) for v in grouped.values())
            print(f"changelog.d/: {n} fragment(s) OK")
        elif args.cmd == "render":
            out = render(load_fragments())
            print(out if out else "(no fragments)")
        elif args.cmd == "apply":
            if not re.match(r"^\d+\.\d+\.\d+$", args.version):
                raise FragmentError(f"--version '{args.version}' is not X.Y.Z")
            if not re.match(r"^\d{4}-\d{2}-\d{2}$", args.date):
                raise FragmentError(f"--date '{args.date}' is not YYYY-MM-DD")
            apply(args.version, args.date, keep=args.keep)
    except FragmentError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
