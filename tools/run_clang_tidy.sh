#!/usr/bin/env bash
# Runs clang-tidy (.clang-tidy) over the project sources for each given PlatformIO env, default: every env that
# compiles different #ifdef code. Fails on any finding. Needs clang-tidy and pio on PATH.
#
# pio check's own clang-tidy run cannot parse the xtensa toolchain headers (its findings include false positives
# from half-parsed standard library types), so this builds a database clang can use (tools/make_tidy_db.py).
set -euo pipefail

cd "$(dirname "$0")/.."

ENVS=("$@")
if [ ${#ENVS[@]} -eq 0 ]; then
    ENVS=(release test-wokwi test-wokwi-release test-wokwi-splash test-wokwi-failinit)
fi

status=0
for env in "${ENVS[@]}"; do
    echo "== clang-tidy: $env"
    pio run -e "$env" -t compiledb > /dev/null
    python3 tools/make_tidy_db.py compile_commands.json ".pio/tidy/$env"
    if ! git ls-files 'src/*.cpp' | xargs clang-tidy --quiet --warnings-as-errors='*' -p ".pio/tidy/$env"; then
        status=1
    fi
done

exit $status
