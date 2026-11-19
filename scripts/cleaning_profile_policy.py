#!/usr/bin/env python3
"""Safety policy for bundled cleaning profiles, and the page that publishes it.

The rule (docs/CLEANING_PROFILES.md §4): a profile may be `safe` only if the
owning app rebuilds every path it lists on its own. This module makes that
rule checkable against `shared/nexis/cleaning_profiles/safety_policy.json`:

* **deny-list** — locations no bundled profile, safe or aggressive, may touch
  (credentials, personal files, installed toolchains, container roots).
* **dangerous classes** — path patterns (app settings, keys, history,
  databases, toolchains, offline data) that a `safe` profile may not touch.

It also renders `docs/WHAT_NEXIS_TOUCHES.md`, the list of every location the
bundled profiles may delete, so the exposure is reviewable without reading
JSON.

Usage:
  cleaning_profile_policy.py            # rewrite docs/WHAT_NEXIS_TOUCHES.md
  cleaning_profile_policy.py --check    # exit 1 if the page is stale or a profile violates the policy
"""

from __future__ import annotations

import argparse
import fnmatch
import json
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
PROFILES_DIR = REPO_ROOT / "shared" / "nexis" / "cleaning_profiles"
POLICY_FILE = PROFILES_DIR / "safety_policy.json"
DOC_FILE = REPO_ROOT / "docs" / "WHAT_NEXIS_TOUCHES.md"
PLATFORMS = ("linux", "macos")

# Placeholders a bundled profile may start with, and what each normalises to
# so `$XDG_CACHE_HOME/x` and `$HOME/.cache/x` compare equal.
ANCHORS = {
    "$HOME": "$HOME",
    "$XDG_CACHE_HOME": "$HOME/.cache",
    "$XDG_CONFIG_HOME": "$HOME/.config",
    "$XDG_DATA_HOME": "$HOME/.local/share",
    "$LIBRARY": "$HOME/Library",
    "$TMPDIR": "$TMPDIR",
}
WILDCARD_CHARS = "*?["


def load_profiles() -> list[dict]:
    profiles = []
    for platform in PLATFORMS:
        for path in sorted((PROFILES_DIR / platform).glob("*.json")):
            profile = json.loads(path.read_text(encoding="utf-8"))
            profile["_file"] = f"{platform}/{path.name}"
            profile.setdefault("safety", "safe")
            profiles.append(profile)
    return profiles


def load_policy() -> dict:
    return json.loads(POLICY_FILE.read_text(encoding="utf-8"))


def normalize(path: str) -> list[str]:
    """Split a profile path into segments with its anchor expanded.

    Returns [] when the path does not start with a recognised anchor.
    """
    if path.startswith("~/"):
        path = "$HOME/" + path[2:]
    path = path.replace("${", "$").replace("}", "")
    head, _, rest = path.partition("/")
    if head not in ANCHORS:
        return []
    return ANCHORS[head].split("/") + [s for s in rest.split("/") if s]


def has_wildcard(segment: str) -> bool:
    return any(c in segment for c in WILDCARD_CHARS)


def segment_matches(profile_seg: str, policy_seg: str) -> bool:
    """Whether a policy segment covers a profile segment.

    A wildcard in the *profile* only matches an identical wildcard in the
    policy: `~/.config/*/Cache` is not flagged for `~/.config/gcloud` merely
    because `*` could expand to it. Those cases are reviewed on the generated
    page instead of failing every generic glob.
    """
    if profile_seg == policy_seg:
        return True
    if has_wildcard(profile_seg):
        return False
    return fnmatch.fnmatchcase(profile_seg, policy_seg)


def policy_covers(policy_segs: list[str], profile_segs: list[str]) -> bool:
    """The policy path is the profile path or one of its ancestors."""
    return len(policy_segs) <= len(profile_segs) and all(
        segment_matches(prof, pol) for pol, prof in zip(policy_segs, profile_segs)
    )


def profile_covers(profile_segs: list[str], policy_segs: list[str]) -> bool:
    """The profile path is the policy path or one of its ancestors."""
    return len(profile_segs) <= len(policy_segs) and all(
        segment_matches(prof, pol) for prof, pol in zip(profile_segs, policy_segs)
    )


def denied_by(entry: dict, path: str) -> list[str]:
    """Deny-list paths of `entry` that `path` would delete or sit inside."""
    segs = normalize(path)
    hits = []
    for denied in entry["paths"]:
        dsegs = normalize(denied)
        if entry["scope"] == "container":
            bare_child = (
                len(segs) == len(dsegs) + 1
                and policy_covers(dsegs, segs)
                and not segs[-1].strip(WILDCARD_CHARS)
            )
            # The profile is the root itself, an ancestor of it, or `root/*`.
            hit = profile_covers(segs, dsegs) or bare_child
        else:
            # Inside the denied tree, or an ancestor that would delete it.
            hit = policy_covers(dsegs, segs) or profile_covers(segs, dsegs)
        if hit:
            hits.append(denied)
    return hits


def class_hits(cls: dict, path: str) -> list[str]:
    """Patterns of `cls` matching any contiguous run of the path's segments."""
    segs = normalize(path)
    hits = []
    for pattern in cls["patterns"]:
        psegs = pattern.split("/")
        for i in range(len(segs) - len(psegs) + 1):
            if all(segment_matches(segs[i + j], psegs[j]) for j in range(len(psegs))):
                hits.append(pattern)
                break
    return hits


def violations(profiles: list[dict], policy: dict) -> list[str]:
    problems = []
    for profile in profiles:
        where = f"{profile['_file']} ({profile['id']})"
        for path in profile["paths"]:
            if not normalize(path):
                problems.append(f"{where}: {path} is not anchored at a known placeholder")
                continue
            for entry in policy["denyList"]:
                for denied in denied_by(entry, path):
                    problems.append(
                        f"{where}: {path} overlaps deny-listed {denied} [{entry['id']}]"
                    )
            if profile["safety"] != "safe":
                continue
            for cls in policy["dangerousClasses"]:
                for pattern in class_hits(cls, path):
                    problems.append(
                        f"{where}: safe profile path {path} matches {cls['id']} pattern '{pattern}'"
                    )
    return problems


def md_code(text: str) -> str:
    return f"`{text}`"


def render(profiles: list[dict], policy: dict) -> str:
    out = [
        "# What Nexis touches",
        "",
        "<!-- GENERATED by scripts/cleaning_profile_policy.py - do not hand-edit. -->",
        "<!-- Regenerate with: python3 scripts/cleaning_profile_policy.py -->",
        "",
        "Every location the bundled System Cleaner profiles may delete, and every",
        "location they are barred from touching. Generated from",
        "`shared/nexis/cleaning_profiles/`; the rule behind the safety labels is in",
        "[`CLEANING_PROFILES.md`](CLEANING_PROFILES.md) §4.",
        "",
        "- **safe** — the owning app rebuilds the path on its own. Cleaned by every scan.",
        "- **aggressive** — losing it costs a re-download, rebuild or offline access.",
        "  Skipped unless the user turns on \"Allow aggressive profiles\".",
        "",
        "Paths are globs; `$HOME` is the user's home directory.",
    ]
    for platform in PLATFORMS:
        rows = [p for p in profiles if platform in p.get("platforms", PLATFORMS)]
        out += ["", f"## {platform.capitalize()} profiles ({len(rows)})", ""]
        out += ["| Profile | App | Safety | Min age | Paths |", "|---|---|---|---|---|"]
        for p in sorted(rows, key=lambda p: (p["safety"] != "safe", p["id"])):
            age = f"{p['minAgeDays']} d" if p.get("minAgeDays") else "-"
            paths = "<br>".join(md_code(x) for x in p["paths"])
            out.append(f"| {md_code(p['id'])} | {p.get('app', p['name'])} | {p['safety']} | {age} | {paths} |")

    out += ["", "## Never touched (deny-list)", "",
            "No bundled profile may overlap these, whatever its safety class.", "",
            "| Class | Location | Scope |", "|---|---|---|"]
    scope_text = {
        "tree": "location and everything under it",
        "container": "the directory itself and a bare wildcard child; entries inside it are fine",
    }
    for entry in policy["denyList"]:
        for denied in entry["paths"]:
            out.append(f"| {entry['id']} | {md_code(denied)} | {scope_text[entry['scope']]} |")

    out += ["", "## Never labelled safe (dangerous classes)", "",
            "A `safe` profile path may not contain a segment run matching these patterns.", "",
            "| Class | Why | Patterns |", "|---|---|---|"]
    for cls in policy["dangerousClasses"]:
        patterns = "<br>".join(md_code(x) for x in cls["patterns"])
        out.append(f"| {cls['id']} | {cls['description']} | {patterns} |")
    return "\n".join(out) + "\n"


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--check", action="store_true", help="fail on a stale page or a policy violation")
    args = ap.parse_args()

    profiles, policy = load_profiles(), load_policy()
    page = render(profiles, policy)

    if not args.check:
        DOC_FILE.write_text(page, encoding="utf-8")
        print(f"wrote {DOC_FILE.relative_to(REPO_ROOT)}")
        return 0

    status = 0
    for problem in violations(profiles, policy):
        print(f"policy violation: {problem}", file=sys.stderr)
        status = 1
    current = DOC_FILE.read_text(encoding="utf-8") if DOC_FILE.exists() else ""
    if current != page:
        print(f"{DOC_FILE.relative_to(REPO_ROOT)} is stale; run: python3 scripts/cleaning_profile_policy.py",
              file=sys.stderr)
        status = 1
    return status


if __name__ == "__main__":
    sys.exit(main())
