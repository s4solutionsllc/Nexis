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
# Matches:
#   **Resolved (abc1234):** note   → hash in bold before colon
#   **Resolved:** (abc1234) note   → hash after colon
#   **Resolved:** note             → no hash
_RESOLVED_BOLD_HASH_RE = re.compile(
    r'\*\*Resolved\s+\(([a-f0-9]{6,10})\):\*\*\s*(.+)',
    re.IGNORECASE | re.DOTALL
)
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
        # Try **Resolved (abc1234):** form first (hash inside bold span)
        bold_m = _RESOLVED_BOLD_HASH_RE.search(block)
        if bold_m:
            commit_hash = bold_m.group(1)
            resolution = bold_m.group(2)[:300].strip()
        else:
            res_m = _RESOLVED_RE.search(block)
            if res_m:
                commit_hash = res_m.group(1)   # commit in parens right after "Resolved:"
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
