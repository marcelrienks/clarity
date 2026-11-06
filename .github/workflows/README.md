# GitHub Actions

One workflow: [`ci.yml`](ci.yml).

| Job | Runs on | What it does |
|---|---|---|
| `build` (matrix: `release`, `debug-upload`, `debug-local`, `test-wokwi`) | every push and pull request | `pio run`, then `pio run -t size` (written to the job summary), uploads `firmware.bin`, `firmware.elf` and `firmware.map` as the `firmware-<env>` artifact (14 days) |
| `wokwi` | pull requests from this repository that are not drafts, and manual runs | downloads the `test-wokwi` firmware and runs `test/wokwi/wokwi_test_automation.sh` (the 5 scenario parts) |

`~/.platformio` is cached, keyed on `platformio.ini`. PlatformIO Core (6.2.0) and Wokwi CLI (v0.29.1) are pinned in the `env:` block of `ci.yml`. A new push to the same ref cancels the run in progress.

## Wokwi job

- Needs the repository secret `WOKWI_CLI_TOKEN` (token from <https://wokwi.com/dashboard/ci>; Settings, Secrets and variables, Actions). The job fails with a clear error if it is missing.
- One full run costs about 7 Wokwi CI minutes against the monthly quota. That is why it skips pushes, drafts and forks (forks get no secrets).
- Wokwi cuts every session at 5 minutes, so the scenario is split into 5 parts (see [docs/test.md](../../docs/test.md)). To debug one part, run `wokwi-cli` on that part locally instead of re-running the whole job.

## Not in CI yet

Lint, `pio check`, `pio test -e native` and the scenario-string check from restructure.md T-04 steps 1 to 3 arrive with phases 2 and 3. There is no size regression gate until a baseline policy exists (`docs/baseline/size.md`).

## Manual run

Actions, CI, Run workflow. This also runs the `wokwi` job.
