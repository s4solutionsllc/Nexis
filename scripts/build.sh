#!/bin/bash
# Build Nexis on macOS or Linux.
#
# Usage:
#   ./scripts/build.sh              # incremental build
#   ./scripts/build.sh clean        # clean rebuild
#   ./scripts/build.sh clean debug  # clean rebuild, Debug config
#
# Args (any order):
#   clean          Remove the build/ directory and reconfigure from scratch.
#   debug          Build CMAKE_BUILD_TYPE=Debug instead of Release.
#   no-tests       Configure with -DBUILD_TESTING=OFF.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="$PROJECT_ROOT/build"

CLEAN=0
BUILD_TYPE="Release"
BUILD_TESTING="ON"

for arg in "$@"; do
    case "$arg" in
        clean) CLEAN=1 ;;
        debug) BUILD_TYPE="Debug" ;;
        no-tests) BUILD_TESTING="OFF" ;;
        *)
            echo "Unknown argument: $arg" >&2
            echo "Usage: $0 [clean] [debug] [no-tests]" >&2
            exit 1
            ;;
    esac
done

case "$(uname -s)" in
    Darwin)
        if ! command -v brew >/dev/null 2>&1; then
            echo "Error: Homebrew not found (required for Qt6 on macOS)." >&2
            exit 1
        fi
        CMAKE_EXTRA_ARGS=(-DCMAKE_PREFIX_PATH="$(brew --prefix qt@6)")
        JOBS="$(sysctl -n hw.ncpu)"
        ;;
    Linux)
        CMAKE_EXTRA_ARGS=()
        JOBS="$(nproc)"
        ;;
    *)
        echo "Error: unsupported platform $(uname -s) (this script supports macOS and Linux only)." >&2
        exit 1
        ;;
esac

if [ "$CLEAN" -eq 1 ]; then
    echo "Removing existing build directory..."
    rm -rf "$BUILD_DIR"
fi

echo "Configuring (CMAKE_BUILD_TYPE=$BUILD_TYPE, BUILD_TESTING=$BUILD_TESTING)..."
cmake -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    -DBUILD_TESTING="$BUILD_TESTING" \
    "${CMAKE_EXTRA_ARGS[@]}"

echo "Building with $JOBS jobs..."
cmake --build "$BUILD_DIR" -j"$JOBS"

echo "Build complete."
