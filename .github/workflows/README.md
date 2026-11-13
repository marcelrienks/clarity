# GitHub Actions

One workflow: [`ci.yml`](ci.yml).

| Job | Runs on | What it does |
|---|---|---|
| `lint` | every push and pull request | `tools/check_format.sh` (clang-format 23.1.5 against `.clang-format`) and `tools/check_layering.sh` (`lib/core` must not include Arduino, LVGL, FreeRTOS or ESP-IDF headers) |
| `check` | every push and pull request | `pio check -e release --fail-on-defect=low` (cppcheck), then `tools/run_clang_tidy.sh` (clang-tidy 23.1.4 with `.clang-tidy` over `release`, `test-wokwi`, `test-wokwi-release`, `test-wokwi-splash` and `test-wokwi-failinit`, so the `#ifdef` test code is checked too) |
| `native-test` | every push and pull request | `pio test -e native`: the host unit tests for `lib/core` |
| `build` (matrix: `release`, `debug-upload`, `debug-local`, `test-wokwi`, `test-wokwi-release`, `test-wokwi-splash`, `test-wokwi-failinit`) | every push and pull request | `pio run`, then `pio run -t size` (written to the job summary), uploads `firmware.bin`, `firmware.elf` and `firmware.map` as the `firmware-<env>` artifact (14 days) |
| `wokwi` | pull requests from this repository that are not drafts, and manual runs | downloads the four `test-wokwi*` firmware artifacts and runs `test/wokwi/wokwi_test_automation.sh` (the 8 scenario parts) |

`~/.platformio` is cached, keyed on `platformio.ini`. PlatformIO Core (6.2.0) and Wokwi CLI (v0.29.1) are pinned in the `env:` block of `ci.yml`. A new push to the same ref cancels the run in progress.

## Wokwi job

- Needs the repository secret `WOKWI_CLI_TOKEN` (token from <https://wokwi.com/dashboard/ci>; Settings, Secrets and variables, Actions). The job fails with a clear error if it is missing.
- One full run costs about 9 Wokwi CI minutes against the monthly quota. That is why it skips pushes, drafts and forks (forks get no secrets).
- Wokwi cuts every session at 5 minutes, so the scenario is split into 8 parts (see [docs/test.md](../../docs/test.md)). To debug one part, run `wokwi-cli` on that part locally instead of re-running the whole job.

## Not in CI yet

The scenario-string check from restructure.md T-04 (every `wait-serial` string exists in `test_events.h`) arrives with phase 9. There is no size regression gate until a baseline policy exists (`docs/baseline/size.md`).

`pio check` does not run clang-tidy here: PlatformIO's own invocation cannot parse the xtensa toolchain headers and reports false positives from half-parsed standard library types. `tools/run_clang_tidy.sh` builds a compile database clang can use (`tools/make_tidy_db.py`) and runs the same `.clang-tidy`. Run it locally with `clang-tidy` and `pio` on `PATH`.

## Manual run

Actions, CI, Run workflow. This also runs the `wokwi` job.
