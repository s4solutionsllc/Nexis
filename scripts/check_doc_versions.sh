#!/usr/bin/env bash
# Verify that the "Version X.Y.Z" header in the living architecture/overview
# docs matches the CMake PROJECT_VERSION. CLAUDE.md's pre-commit checklist
# requires these be kept in sync; this script enforces it in CI.
#
# Usage: scripts/check_doc_versions.sh
# Exits 0 on match, 1 on mismatch or missing fields.
#
# WI-30 (SSO-3392): added to catch the kind of drift that motivated the
# audit reconciliation.

set -euo pipefail

repo_root="$(cd "$(dirname "$0")/.." && pwd)"
cd "$repo_root"

# Use grep -oE (POSIX ERE) so this works under both GNU and BSD userlands;
# `sed -E` with `\+` is GNU-only and silently fails to match on macOS.
cmake_version="$(grep -oE 'project\([^)]*VERSION [0-9]+\.[0-9]+\.[0-9]+' CMakeLists.txt \
  | head -1 \
  | grep -oE '[0-9]+\.[0-9]+\.[0-9]+')"
if [[ -z "$cmake_version" ]]; then
  echo "check_doc_versions: could not parse VERSION from CMakeLists.txt project()" >&2
  exit 1
fi

mismatch=0

check_doc() {
  local doc="$1"
  if [[ ! -f "$doc" ]]; then
    echo "check_doc_versions: $doc not found" >&2
    mismatch=1
    return
  fi

  # Find a line like "Version 2.3.13" (with optional leading "> Last updated: ... |")
  local doc_version
  doc_version="$(grep -oE 'Version [0-9]+\.[0-9]+\.[0-9]+' "$doc" | head -1 | awk '{print $2}')"

  if [[ -z "$doc_version" ]]; then
    echo "check_doc_versions: $doc has no 'Version X.Y.Z' header" >&2
    mismatch=1
    return
  fi

  if [[ "$doc_version" != "$cmake_version" ]]; then
    echo "check_doc_versions: $doc header is Version $doc_version, expected $cmake_version (from CMakeLists.txt)" >&2
    mismatch=1
    return
  fi

  echo "check_doc_versions: $doc OK (Version $doc_version)"
}

check_doc "docs/APPLICATION_OVERVIEW.md"
check_doc "docs/ARCHITECTURE_REVIEW.md"

if [[ "$mismatch" -ne 0 ]]; then
  echo "check_doc_versions: one or more docs are out of sync with PROJECT_VERSION ($cmake_version)." >&2
  echo "  Update the 'Last updated: YYYY-MM-DD | Version X.Y.Z' header in each listed file." >&2
  exit 1
fi
