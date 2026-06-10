#!/usr/bin/env bash
#
# WI-27 / SSO-3389: forbid platform-specific Info headers in shared/Pages.
#
# `shared/nexis/Pages/**` is the cross-platform UI tier. It must reach
# platform-specific Info subclasses through the InfoManager facade — never
# by including `*_macos.h` or `*_linux.h` directly. Each direct include is
# another site to edit when adding a new platform.
#
# This script greps the Pages tree for forbidden includes and exits non-zero
# if any are found.

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PAGES_DIR="$REPO_ROOT/shared/nexis/Pages"

if [ ! -d "$PAGES_DIR" ]; then
    echo "error: expected directory $PAGES_DIR not found" >&2
    exit 2
fi

# Match `#include <…_macos.h>`, `#include "…_linux.h"`, plus the leading-space
# variants. Restrict to .h / .cpp / .mm sources.
PATTERN='^[[:space:]]*#[[:space:]]*include[[:space:]]*[<"][^<>"]*(_macos|_linux)\.h[>"]'

matches=$(grep -REn --include='*.h' --include='*.cpp' --include='*.mm' \
    "$PATTERN" "$PAGES_DIR" || true)

if [ -n "$matches" ]; then
    echo "error: shared/nexis/Pages must not include *_macos.h / *_linux.h directly." >&2
    echo "       Use the InfoManager facade (or a createForPlatform() factory)." >&2
    echo "" >&2
    echo "Offending lines:" >&2
    echo "$matches" >&2
    exit 1
fi

echo "ok: shared/nexis/Pages contains no platform-specific Info header includes"
