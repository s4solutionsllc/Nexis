#!/usr/bin/env python3
"""Nightly installation-stats collector (install-stats.yml).

Aggregates three sources into one dated snapshot appended to
website/src/data/install-stats.json:

  1. GitHub Releases API  — cumulative per-asset download_count, classified
     into channels by filename (see classify()).
  2. Launchpad API        — per-binary download counts for the
     ppa:s4solutionsllc/nexis archive → the `apt` channel.
  3. AUR RPC              — NumVotes / Popularity (supplementary signal only;
     actual AUR install counts come from the -source.tar.gz release asset).

`totals` are cumulative downloads across all releases; `latestVersion` counts
only the latest release's assets — the best available proxy for the *active*
install base (strongest for apt, weakest for AppImage; see
docs/plans/2026-07-05-installation-tracking-findings-and-plan.md §1.3).

Stdlib only — runs on a bare ubuntu-latest runner. Fails loudly on any source
error rather than recording a partial (misleadingly low) snapshot.
"""

import json
import os
import sys
import urllib.request
from datetime import datetime, timezone
from pathlib import Path

REPO = os.environ.get("GITHUB_REPOSITORY", "s4solutionsllc/Nexis")
TOKEN = os.environ.get("GITHUB_TOKEN", "")
DATA_FILE = Path(__file__).resolve().parent.parent / "website/src/data/install-stats.json"

CHANNELS = ["deb-direct", "appimage", "dmg-direct", "brew", "apt", "aur"]
LAUNCHPAD_ARCHIVE = "https://api.launchpad.net/devel/~s4solutionsllc/+archive/ubuntu/nexis"
AUR_RPC = "https://aur.archlinux.org/rpc/v5/info?arg[]=nexis"


def fetch_json(url, github=False):
    req = urllib.request.Request(url, headers={"User-Agent": "nexis-install-stats"})
    if github and TOKEN:
        req.add_header("Authorization", f"Bearer {TOKEN}")
    with urllib.request.urlopen(req, timeout=60) as resp:
        return json.load(resp)


def classify(asset_name):
    """Map a release-asset filename to a channel, or None if uncounted.

    Order matters: .brew.dmg before .dmg. Raw binaries (nexis-x86_64 /
    nexis-arm64) and SHA256SUMS files are deliberately uncounted — they are
    not an installation channel.
    """
    if asset_name.endswith(".brew.dmg"):
        return "brew"
    if asset_name.endswith(".dmg"):
        return "dmg-direct"
    if asset_name.endswith(".deb"):
        return "deb-direct"
    if asset_name.endswith(".AppImage"):
        return "appimage"
    if asset_name.endswith("-source.tar.gz"):
        return "aur"
    return None


def asset_arch(asset_name):
    lowered = asset_name.lower()
    if "aarch64" in lowered or "arm64" in lowered:
        return "arm64"
    return "x86_64"


def github_release_counts():
    """Return (totals_by_channel, latest_version, latest_by_channel, appimage_by_arch)."""
    totals = {c: 0 for c in CHANNELS}
    appimage_by_arch = {"x86_64": 0, "arm64": 0}

    latest = fetch_json(f"https://api.github.com/repos/{REPO}/releases/latest", github=True)
    latest_version = latest["tag_name"].lstrip("v")
    latest_by_channel = {c: 0 for c in CHANNELS}
    for asset in latest.get("assets", []):
        channel = classify(asset["name"])
        if channel:
            latest_by_channel[channel] += asset["download_count"]

    page = 1
    while True:
        releases = fetch_json(
            f"https://api.github.com/repos/{REPO}/releases?per_page=100&page={page}",
            github=True,
        )
        if not releases:
            break
        for release in releases:
            if release.get("draft"):
                continue
            for asset in release.get("assets", []):
                channel = classify(asset["name"])
                if not channel:
                    continue
                totals[channel] += asset["download_count"]
                if channel == "appimage":
                    appimage_by_arch[asset_arch(asset["name"])] += asset["download_count"]
        page += 1

    return totals, latest_version, latest_by_channel, appimage_by_arch


def launchpad_counts(latest_version):
    """Return (total_downloads, latest_version_downloads, by_series)."""
    total = 0
    latest = 0
    by_series = {}
    url = f"{LAUNCHPAD_ARCHIVE}?ws.op=getPublishedBinaries&ws.size=75"
    while url:
        page = fetch_json(url)
        for entry in page.get("entries", []):
            count = fetch_json(entry["self_link"] + "?ws.op=getDownloadCount")
            if not isinstance(count, int):
                count = 0
            total += count
            # distro_arch_series_link: .../devel/ubuntu/<series>/<arch>
            series_parts = entry.get("distro_arch_series_link", "").rstrip("/").split("/")
            if len(series_parts) >= 2:
                series = series_parts[-2]
                by_series[series] = by_series.get(series, 0) + count
            # Deb versions look like `2.8.2-1~questing1` — prefix-match the tag.
            if entry.get("binary_package_version", "").startswith(latest_version):
                latest += count
        url = page.get("next_collection_link")
    return total, latest, by_series


def aur_info():
    data = fetch_json(AUR_RPC)
    results = data.get("results") or []
    if not results:
        return {"votes": 0, "popularity": 0.0}
    return {
        "votes": results[0].get("NumVotes", 0),
        "popularity": results[0].get("Popularity", 0.0),
    }


def main():
    totals, latest_version, latest_by_channel, appimage_by_arch = github_release_counts()
    apt_total, apt_latest, apt_by_series = launchpad_counts(latest_version)
    totals["apt"] = apt_total
    latest_by_channel["apt"] = apt_latest

    snapshot = {
        "date": datetime.now(timezone.utc).strftime("%Y-%m-%d"),
        "totals": {
            "all": sum(totals.values()),
            "byChannel": totals,
        },
        "latestVersion": {
            "version": latest_version,
            "byChannel": latest_by_channel,
        },
        "aur": aur_info(),
        "detail": {
            "appimageByArch": appimage_by_arch,
            "aptBySeries": apt_by_series,
        },
    }

    history = []
    if DATA_FILE.exists():
        history = json.loads(DATA_FILE.read_text() or "[]")
    # Re-running on the same day replaces that day's snapshot (idempotent).
    history = [s for s in history if s.get("date") != snapshot["date"]]
    history.append(snapshot)
    history.sort(key=lambda s: s["date"])

    DATA_FILE.parent.mkdir(parents=True, exist_ok=True)
    DATA_FILE.write_text(json.dumps(history, indent=2) + "\n")

    print(f"Snapshot for {snapshot['date']}: total={snapshot['totals']['all']}")
    print(json.dumps(snapshot, indent=2))
    return 0


if __name__ == "__main__":
    sys.exit(main())
