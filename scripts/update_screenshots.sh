#!/bin/bash
# Regenerate reference screenshots for the current platform.
#
# Usage:
#   ./scripts/update_screenshots.sh
#
# Prerequisites:
#   - Project must already be built: cmake --build build
#   - On headless Linux, xvfb must be installed (xvfb-run is used automatically)
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

# Use xvfb-run on Linux if no display is available
RUN_CMD="$TEST_BIN"
if [ "$(uname)" = "Linux" ] && [ -z "${DISPLAY:-}" ]; then
    if command -v xvfb-run >/dev/null 2>&1; then
        RUN_CMD="xvfb-run -a $TEST_BIN"
    else
        echo "Warning: No DISPLAY and xvfb-run not found. Screenshots may fail."
    fi
fi

echo "Generating reference screenshots..."
NEXIS_GENERATE_REFS=1 $RUN_CMD

echo ""
echo "Done. Review changes with:"
echo "  git diff --stat tests/reference_screenshots/"
echo ""
echo "If the changes look correct, commit them:"
echo "  git add tests/reference_screenshots/"
echo "  git commit -m 'chore: update reference screenshots'"
