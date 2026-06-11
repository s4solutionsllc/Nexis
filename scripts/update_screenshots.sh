#!/bin/bash
# Regenerate reference screenshots for the current platform.
#
# Usage:
#   ./scripts/update_screenshots.sh
#
# Prerequisites:
#   - Project must already be built: cmake --build build
#   - Headless runs work out of the box via the Qt offscreen QPA — no Xvfb
#     required (important on Wayland-only hosts like Ubuntu 26.04 / GNOME 50,
#     where the X11 session is removed; SSO-3729 / FW-02).
#
# The script runs the screenshot test in generate mode, which captures all 11
# pages in both Dark and Light themes and saves them as reference PNGs under
# tests/reference_screenshots/{platform}/{theme}/.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

TEST_BIN="$PROJECT_ROOT/build/output/test-ScreenshotTests"

if [ ! -x "$TEST_BIN" ]; then
    echo "Error: test-ScreenshotTests not found at $TEST_BIN"
    echo "Build the project first:  cmake --build build"
    exit 1
fi

cd "$PROJECT_ROOT/build"

# SSO-3729 / FW-02: prefer Qt's offscreen QPA when no display socket is
# present. Works on every Linux session type — Wayland-only (GNOME 50 /
# Ubuntu 26.04, where the X11 session is removed entirely), X11, or fully
# headless CI — and matches the offscreen seam main.cpp uses for scheduled
# --clean runs (SSO-3368). A live DISPLAY or WAYLAND_DISPLAY is respected
# so a developer running locally sees rendered output as usual; an
# operator-set QT_QPA_PLATFORM (e.g. `xcb` for explicit XWayland) is also
# respected.
if [ "$(uname)" = "Linux" ] \
   && [ -z "${DISPLAY:-}" ] && [ -z "${WAYLAND_DISPLAY:-}" ] \
   && [ -z "${QT_QPA_PLATFORM:-}" ]; then
    export QT_QPA_PLATFORM=offscreen
fi

echo "Generating reference screenshots..."
NEXIS_GENERATE_REFS=1 "$TEST_BIN"

echo ""
echo "Done. Review changes with:"
echo "  git diff --stat tests/reference_screenshots/"
echo ""
echo "If the changes look correct, commit them:"
echo "  git add tests/reference_screenshots/"
echo "  git commit -m 'chore: update reference screenshots'"
