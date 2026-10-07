#!/usr/bin/env bash
# Runs clang-tidy on every project source file.
# Requires a configured build directory (for compile_commands.json).
#
# Usage:
#   scripts/lint.sh [build-dir]   # defaults to build/debug

set -euo pipefail

cd "$(dirname "$0")/.."

build_dir="${1:-build/debug}"

if [[ ! -f "${build_dir}/compile_commands.json" ]]; then
    echo "No compile_commands.json in '${build_dir}'. Configure first: cmake --preset debug" >&2
    exit 1
fi

source_dirs=()
for dir in src app tests; do
    [[ -d "${dir}" ]] && source_dirs+=("${dir}")
done

find "${source_dirs[@]}" -type f -name '*.cpp' \
    | xargs --no-run-if-empty clang-tidy -p "${build_dir}" --quiet \
        --extra-arg=-Wno-unknown-warning-option
