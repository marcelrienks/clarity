# Size baseline (task 0.2)

Reference for the no-regression budget in [todo.md](../todo.md). Captured 2026-11-05 from the pre-restructure code (branch `v2`, commit `07a24e6`).

## Build configuration

`platformio.ini` was not yet changed in the repo. The baseline was built from a copy with only the changes recorded in [decisions.md](../decisions.md) (Q-11) applied:

- `platform = platformio/espressif32 @ 8.0.0`
- `lvgl/lvgl @ 9.3.0`, `lovyan03/LovyanGFX @ 1.2.7`
- added `-D LV_KCONFIG_IGNORE`
- removed `-D ARDUINO_USB_MODE=1` and `-D ARDUINO_USB_CDC_ON_BOOT=1`

Toolchain: Arduino core 3.3.12, ESP-IDF 5.5.5, GCC 14.2.0. Commands: `pio run -e <env>` and `pio run -e <env> -t size`.

## Results

App partition is 2,097,152 B (2 MB). RAM is 327,680 B.

| Env | Flash used | Flash % | RAM used | RAM % | `firmware.bin` | text | data | bss |
|---|---|---|---|---|---|---|---|---|
| `release` | 1,351,723 | 64.5% | 35,436 | 10.8% | 1,395,296 | 854,627 | 540,548 | 16,697 |
| `debug-local` | 1,462,759 | 69.7% | 35,716 | 10.9% | 1,510,656 | 955,055 | 555,492 | 16,977 |
| `debug-upload` | 1,458,879 | 69.6% | 35,460 | 10.8% | 1,506,672 | 953,471 | 553,088 | 16,721 |
| `test-wokwi` | 1,462,759 | 69.7% | 35,716 | 10.9% | 1,510,656 | 955,055 | 555,492 | 16,977 |

`debug-local` and `test-wokwi` produce identical sizes. `text`/`data`/`bss` come from `xtensa-esp32-elf-size` on `firmware.elf`.

## Not in the repo

The ELF files and linker maps (about 109 MB) are kept outside the repo in `~/Workspaces/personal/clarity-baseline-0.2/` (`<env>.elf`, `<env>.map.gz`). They are not committed.

## Serial log

`serial-test-wokwi.txt` is the `wokwi-cli` output of the `test-wokwi` firmware (`firmware.bin`) against `scenario_automated.yaml`, 2026-11-05, wokwi-cli 0.29.1. It includes the scenario's `Expected text matched` lines.

The scenario does not complete. 22 of 61 `wait-serial` steps matched. It stalls at `scenario_automated.yaml:104` (`[T] Error reported: ERROR`): the firmware prints that line during the preceding 500 ms `delay`, before the scenario starts waiting for it, so the step never matches. After that the firmware only prints the GPIO poll lines until the run is cut off. This is the real starting point for 0.3, and it is earlier than the 9 stale strings listed there (first at line 121).

Run from a directory without `wokwi.toml`, or the simulation waits for a debugger on port 3333 (`gdbServerPort` in `test/wokwi/wokwi.toml`).
