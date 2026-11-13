#!/usr/bin/env bash
# Fails if any project source is not clang-format clean (uses .clang-format). Third-party code is skipped.
set -euo pipefail

cd "$(dirname "$0")/.."

git ls-files 'src/*.cpp' 'src/*.h' 'include/*.h' 'lib/*.h' 'lib/*.cpp' 'test/*.cpp' 'test/*.h' |
    grep -v -E '^include/(icons/|lv_conf\.h$)' |
    xargs clang-format --dry-run --Werror

echo "check_format: OK"
