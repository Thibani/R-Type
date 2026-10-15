#!/usr/bin/env bash
# Formats every C++ source of the project with clang-format.
#
# Usage:
#   scripts/format.sh          # rewrite files in place
#   scripts/format.sh --check  # only report badly formatted files (for CI)

set -euo pipefail

cd "$(dirname "$0")/.."

mode=(-i)
if [[ "${1:-}" == "--check" ]]; then
    mode=(--dry-run --Werror)
fi

source_dirs=()
for dir in src app tests; do
    [[ -d "${dir}" ]] && source_dirs+=("${dir}")
done

find "${source_dirs[@]}" -type f \( -name '*.hpp' -o -name '*.cpp' \) \
    | xargs --no-run-if-empty clang-format "${mode[@]}"
