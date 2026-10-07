#!/usr/bin/env bash
# Configures, builds and runs the whole test suite.
#
# Usage:
#   scripts/test.sh [preset]   # defaults to "debug"

set -euo pipefail

cd "$(dirname "$0")/.."

preset="${1:-debug}"

cmake --preset "${preset}"
cmake --build --preset "${preset}"
ctest --preset "${preset}"
