#!/usr/bin/env bash
# CI / developer build script. Configures and builds everything, then runs
# the unit tests. Must pass without a game directory present (CLAUDE.md §7).
set -euo pipefail

BUILD_DIR="${BUILD_DIR:-build}"
BUILD_TYPE="${BUILD_TYPE:-RelWithDebInfo}"

cmake -S "$(dirname "$0")/.." -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
    "$@"
cmake --build "$BUILD_DIR" --parallel "$(nproc)"
ctest --test-dir "$BUILD_DIR" --output-on-failure
