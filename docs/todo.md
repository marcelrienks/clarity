# TODO: Restructure Implementation Plan

Step-by-step plan for the restructure specified in [restructure.md](restructure.md). That document is the source of truth for *what* and *why*. This file is the *order of work*.

**Item IDs** (`D-` defect, `A-` architecture, `C-` code, `B-` build, `T-` test, `H-` hardening, `X-` docs, `Q-` decision) refer to `restructure.md`.

**Rules for every phase**

- Each phase leaves the device working and is merged only when its exit criteria are met.
- Common exit criteria: all builds green, native tests green (once they exist), Wokwi scenario green, no unplanned behaviour change against restructure.md §2, no flash/RAM regression beyond budget.
- Any change to a `[T]` log string updates the Wokwi scenario in the same commit (§2.9).
- Behaviour-changing items are gated on their `Q-` decision. Record each outcome as one line in `docs/decisions.md` (X-10).

**Phase overview**

| Phase | Goal | Depends on |
|---|---|---|
| 0 | Safety net: toolchain decision, repaired scenario, CI, pinned builds | none |
| 1 | Fix critical defects in place | 0 |
| 2 | Tooling: warnings, formatting, static analysis, `lib/core` + native env | 0 |
| 3 | Extract the pure core with host tests | 2 |
| 4 | Input and display-policy cut-over, deferred transitions | 1, 3 |
| 5 | UI layer rebuild | 4 |
| 6 | Typed settings, NVS-compatible | 5 |
| 7 | Composition root, remove singletons and factories | 6 |
| 8 | Sensing semantics | 7 |
| 9 | Assets, config trim, partitions, docs | 8 |

---

## Phase 0: Safety net

Goal: a trustworthy build and test baseline before any code changes.

- [x] **0.1 Decide the toolchain (Q-11).** Run `pio run -v` on `release` and record: platform version, Arduino core version, GCC version, C++ standard, whether `-fno-exceptions` is set (C-06). This fixes the C++ standard and what `lib/core` may use (for example float `std::from_chars`).
  - [x] Check which espressif32 version actually builds today (a local install showed platform 8.0.0 / Arduino core 3.x, not 6.x).
  - [x] If moving to pioarduino, confirm LovyanGFX and LVGL build and run on it.
  - [x] Write the outcome to `docs/decisions.md`.
- [x] **0.2 Record the baseline.** Capture a golden serial log from `test-wokwi`, plus `pio run -t size` output and the linker map for each env.
  - [x] Size output and linker map for each env (see `docs/baseline/size.md`; maps kept outside the repo).
  - [x] Golden serial log from `test-wokwi` (`docs/baseline/serial-test-wokwi.txt`). The scenario stalls at step 104 (22 of 61 `wait-serial` matched), see `docs/baseline/size.md`.
- [x] **0.3 Repair the Wokwi scenario (D-22).**
  - [x] List the 9 `wait-serial` strings in `test/wokwi/scenario_automated.yaml` that the firmware no longer prints (`Button action detected: SHORT_PRESS` / `LONG_PRESS`, 7 `ConfigPanel: …` messages).
  - [x] Either restore the log lines or update the scenario to match the current output. The 2 button strings now match `ShortPressActivate()` / `Long press triggered during hold`. The 7 `ConfigPanel:` messages were restored as `log_t` lines in `config_panel.cpp`. Poll logs in `gpio_sensor.cpp` are gated behind `GPIO_POLL_LOGS`.
  - [x] Make the runner exit non-zero on timeout. The runner already did; it now also loops over the scenario parts and exits 1 if any part fails (verified on a timeout).
  - [x] Run end to end and confirm it passes. The scenario no longer fits Wokwi's 5 minute session cap, so it is split into 5 parts (`scenario_automated_1..5_*.yaml`, see `docs/test.md`). All 5 pass through `wokwi_test_automation.sh` (about 7 minutes in total).
- [x] **0.4 Pin versions (B-01).** Pin `platform`, `lvgl/lvgl` (9.3.0) and `lovyan03/LovyanGFX` (1.2.7) to exact versions; make README match.
  - [x] `platform = espressif32 @ 8.0.0`, `lvgl/lvgl @ 9.3.0`, `lovyan03/LovyanGFX @ 1.2.7`. ArduinoJson (7.4.3) and Unity (2.6.1) pinned too until 0.5 removes them.
  - [x] Added `-D LV_KCONFIG_IGNORE` and removed `ARDUINO_USB_MODE` / `ARDUINO_USB_CDC_ON_BOOT`, both needed for the pins to build on core 3.3.12 (see `docs/decisions.md`). That covers the USB item in 0.5.
  - [x] All four envs build from a clean copy, sizes identical to the `decisions.md` table (`release` 1,351,723 B flash).
  - [x] README matches (LVGL 9.3.0, LovyanGFX 1.2.7, ArduinoJson 7.4.3, platform/core versions).
- [x] **0.5 Clean `platformio.ini` (§7.1).**
  - [x] Remove ArduinoJson (unused). The stray `#include <ArduinoJson.h>` in `storage_provider.h` is gone too; README entry removed.
  - [x] Remove Unity from `release` and `debug-upload` `lib_deps`.
  - [x] Remove `-ffast-math`.
  - [x] Remove `-I lib`, `-I test`. `-I include` stays: LVGL builds as a library and finds `lv_conf.h` only through it (PlatformIO adds `include/` for project sources only, so §7.1 is wrong on this point).
  - [x] Remove obsolete flags: `LV_TICK_CUSTOM`, `LV_USE_FS_FATFS`, `LV_MEM_SIZE`/`LV_MEM_ADR` (until B-02), `ESP32=1`, the `+<../hal/esp32>` and `-<**/draw/nxp/vglite/**>` filters, the duplicate section-gc flags. (`ARDUINO_USB_*` already removed in 0.4.)
  - [x] Switch `-O0` to `-Og` in debug envs.
  - [x] Add `monitor_filters = esp32_exception_decoder`.
  - [x] Keep env names (`release`, `debug-upload`, `debug-local`, `test-wokwi`) for now, since `wokwi*.toml`, the test script and `.vscode/tasks.json` depend on them.
  - [x] Rebuild all four envs; compare size against the 0.2 baseline. All build. Flash: `release` 1,339,299 B (-12,424), `debug-upload` 1,417,643 B (-41,236), `debug-local` and `test-wokwi` 1,421,731 B (-41,280); RAM -280 to -368 B. The Wokwi scenario passes on the `-Og` build (5 parts, 63 to 108 s each).
- [x] **0.6 Enable LVGL asserts in debug.** `LV_USE_ASSERT_OBJ 1` (and `LV_USE_ASSERT_MEM_INTEGRITY 1`) in debug builds only. Switch Oil → Config and record whether D-01 triggers.
  - [x] `lv_conf.h`: both asserts on under `CLARITY_DEBUG`, off otherwise. In debug `LV_ASSERT_HANDLER` is `abort();` (panic backtrace, decodable with `esp32_exception_decoder`) instead of the silent `while(1);`; release keeps the halt. `release` size unchanged (1,339,299 B), debug envs +6.4 KB.
  - [x] Wokwi scenario on `test-wokwi`: 5 of 5 parts pass; parts 3 to 5 go Oil → Config and back. No assert, abort, backtrace or unexpected reset in the log (only the 5 power-on resets at part starts). D-01 does **not** trigger on this path. `ASSERT_OBJ` can miss a delete of an already-freed object if the memory is not reused, so D-01 stays open until 1.2 fixes it.
- [x] **0.7 Restore CI (T-04 steps 2, 4, 5).**
  - [x] Added `.github/workflows/ci.yml`: `build` matrix over all four envs (`pio run`, `pio run -t size` to the job summary, `firmware.bin`/`.elf`/`.map` uploaded as artifacts), and a `wokwi` job that runs `wokwi_test_automation.sh` (5 parts, about 7 CI minutes) on non-draft pull requests from this repo and on manual runs. It fails fast if `WOKWI_CLI_TOKEN` is not set. Forks and pushes skip it to save quota.
  - [x] `~/.platformio` cached on the `platformio.ini` hash; triggers on push and pull request. PlatformIO Core 6.2.0 and Wokwi CLI v0.29.1 pinned. The Linux CLI asset is `wokwi-cli-linuxstatic-x64` (the `wokwi-cli-linux` URL in `docs/test.md` was dead; fixed).
  - [x] Rewrote `.github/workflows/README.md` (it described three workflows that did not exist).
  - [x] Verified on GitHub (draft PR #55, v2 to main, marked ready once for the Wokwi job). All 4 builds green on push and on the PR. Wokwi job green: 5 of 5 parts pass (54, 88, 87, 88, 88 s, about 7 min). CI sizes: `release` 1,339,283 B (local 1,339,299 B), `debug-upload` 1,424,107 B, `debug-local` and `test-wokwi` 1,428,147 B; the debug increase over the 0.5 numbers is the 0.6 asserts. The phase exit "CI green on `main`" can only be met at merge, so the PR stays an unmerged draft until the todo is done; mark it ready only to run the Wokwi job.
- [x] **0.8 Fix broken tooling references.** `.vscode/tasks.json` points at the missing `run_automated_tests.sh`. README references deleted `wokwi_debug.sh` and `wokwi_run.sh` (D-22).
  - [x] The `.vscode/tasks.json` Wokwi task now runs `test/wokwi/wokwi_test_automation.sh`.
  - [x] README "Testing & Simulation" now shows the `test-wokwi` build plus `wokwi_test_automation.sh`; the two deleted scripts are no longer referenced.
- [x] **0.9 Delete** the 394 KB capture `test/wokwi/wikwi-manual-test.txt` (§7.6). Removed with `git rm`; nothing referenced it. The golden log for the baseline lives in `docs/baseline/serial-test-wokwi.txt`.

**Exit:** CI green on `main`; scenario passes end to end; toolchain recorded.

---

## Phase 1: Critical fixes in place

Goal: remove S1 defects with minimal patches, before restructuring. Add a Wokwi case for each fix.

- [x] **1.1 Add the test-only error injection hook (T-03).** Compile into the Wokwi env only (for example `CLARITY_TEST_INJECT_ERROR`, or a scenario-driven input). Needed to test D-03 without relying on D-05e.
  - [x] `CLARITY_TEST_INJECT_ERROR` (`utilities/test_inject.h`, `src/utilities/test_inject.cpp`): a rising edge on GPIO 34 reports one `ERROR` via `ErrorManager`; no-op inline stubs without the flag, and a `#error` if combined with `CLARITY_DEBUG`. Hooked into `setup()` and `loop()` in `main.cpp`. A serial-driven input was rejected: the backlight PWM owns GPIO 3 (UART0 RX).
  - [x] New env `test-wokwi-release` (no `CLARITY_DEBUG`, `-Os`, `TEST_LOGS`, `WOKWI_EMULATOR`, the flag), added to the CI build matrix. D-03 only reproduces without `CLARITY_DEBUG`, so `test-wokwi` cannot host it. `release` is byte-identical in size (1,339,299 B); `test-wokwi-release` is 1,343,335 B.
  - [x] Exercised in Wokwi by 1.4: `scenario_automated_6_error_release.yaml` runs on `test-wokwi-release`.
- [x] **1.2 D-01: LVGL double delete.**
  - [x] Remove `lv_obj_del` of `oilIcon_`, `lowLabel_`, `highLabel_`, `pivotCircle_`, `pivotHighlight_` in `~OemOilComponent`; they are children of `scale_`. The needle lines are children of `scale_` too, so the destructor is now `= default` and deletes nothing.
  - [x] Stop deleting `scale_` there; only the panel's `screen_` is deleted.
  - [x] Make `screen_` the single owner of its LVGL children, so deletion order no longer matters. `KeyComponent` and `LockComponent` deleted their icons (children of `screen_`) in their destructors; both are now `= default`. No component in any of the six panels deletes LVGL objects now, so `OemOilPanel`, `ErrorPanel`, `ConfigPanel` and `SplashPanel` deleting `screen_` in the destructor body (before member destruction) is safe. `ErrorPanel` and `SplashPanel` also null `screen_` / `blankScreen_`. `grep lv_obj_del` shows only panel screen deletes remain.
  - [x] Verify with the LVGL asserts from 0.6. `test-wokwi` (asserts on): 5 of 5 parts pass (part 5 needed one rerun after a Wokwi `503`); parts 3 to 5 go Oil → Config and back with no assert, abort or backtrace. Re-run after the screen-ownership change: `release` and `test-wokwi` build, 5 of 5 parts pass (85, 136, 142, 111, 133 s), no assert, abort or backtrace in the log.
- [x] **1.3 D-02: splash timers.**
  - [x] One rolling `timer_` handle instead of three: the timers are one-shot and chained (fade-in → display → fade-out), so only one is pending at a time. `~SplashPanel` deletes it if pending. Each callback sets `timer_ = nullptr` on entry because it deletes its own timer; this matters most in the fade-out callback, where `OnPanelLoadComplete` may destroy the panel and the destructor would otherwise delete the firing timer a second time.
  - [x] `blankScreen_` initialised to `nullptr` (`splash_panel.h`). Verified: `release` and `test-wokwi` build; Wokwi 5 of 5 parts pass (84, 139, 144, 123, 133 s), no assert, abort or backtrace. No scenario case added, since the timer leak needs a splash destroyed mid-animation, which no current path does.
  - [x] Wokwi part 7 (`test-wokwi-splash`, added in `4f5c8e3` but first run in the Phase 0 to 2 review) found a second defect on the same path: destroying the splash while its fade-in screen load was still pending left LVGL holding the freed screen as `scr_to_load`, so the next `lv_screen_load` hit the `LV_ASSERT_OBJ` in `lv_obj_set_pos` (debug builds abort; release would dereference freed memory). `~SplashPanel` now frees both screens with `lv_obj_delete_async` while `lv_anim_get(screen_)` shows a pending load, so the next load finishes the animation first. The scenario also expected `Pressure reading:` after `OemOilPanel loaded successfully`, but the firmware prints it before; reordered. Part 7 passes (50 s); it aborted in a reboot loop before the fix.
- [x] **1.4 D-03: error panel lock-up.**
  - [x] `ErrorPanel::Update` now calls `CheckRestoration()` when the list becomes empty in every build (the `#ifdef CLARITY_DEBUG` is gone). If nothing replaced the panel (no restoration target), it sets the UI IDLE, so it always ends in a defined state. It keeps the manager pointer in a local, since `CheckRestoration()` normally destroys the panel.
  - [x] Wokwi case: new part 6, `scenario_automated_6_error_release.yaml`. Injected error (GPIO 34) → short press views the only error → `CheckRestoration() called`, restore to `OemOilPanel` → long press opens Config, proving input works. Runs on `test-wokwi-release`: `wokwi_test_automation.sh` picks that firmware for any `*_release.yaml`, and CI downloads the `firmware-test-wokwi-release` artifact for the Wokwi job. Confirmed it fails without the fix (stalls after the short press, no `CheckRestoration()`, until the 5 minute cap) and passes with it. Full suite: 6 of 6 parts pass (81, 145, 143, 124, 132, 104 s). Wokwi CI cost is now about 9 minutes per run.
- [x] **1.5 D-04: failed init.** Minimal guard: skip `loop()` work if `initializeServices()` failed, then `esp_restart()` after a delay. The full fatal state (H-01) arrives in Phase 7.
  - [x] `setup()` records `initFailed` and the time when `initializeServices()` fails; `loop()` returns before touching any manager and calls `esp_restart()` after 5 s (`INIT_FAILED_RESTART_DELAY_MS`). No Wokwi case: nothing can force an init failure without a new hook. `release`, `test-wokwi` and `test-wokwi-release` build. Full suite: 6 of 6 pass on rerun (85, 139, 146, 126, 131, 102 s); the first run failed parts 2 and 3, not reproduced on rerun.
- [x] **1.6 D-07: DiagnosticPanel.** Remove it from `PANEL_OPTIONS` (Q-10) and validate the stored default panel on load. Wokwi case: a stored `DiagnosticPanel` value falls back to Oil.
  - [x] `DiagnosticPanel` removed from `PANEL_OPTIONS`, `PanelNames` and `docs/config.md`; Q-10 recorded in `docs/decisions.md`. `StorageProvider::LoadConfigSection` checks every stored `Selection` value against its allowed options; an invalid one keeps the default and logs `[T] Invalid stored value for <section>.<key>, using default`.
  - [x] Wokwi case in part 6 (`test-wokwi-release`): `CLARITY_TEST_SEED_INVALID_PANEL` seeds `DiagnosticPanel` as the stored default panel at boot; the scenario expects the invalid-value line after `System ready`, then the boot reaches `OemOilPanel`. Part 6 passes (95 s). Only part 6 was run, since the change touches storage load and the part 6 firmware; the full suite is left for the end of the phase. `release`, `test-wokwi` and `test-wokwi-release` build (`release` 1,339,167 B).
  - [x] `wokwi_test_automation.sh` now takes part numbers (`... 6`) to run only the parts covering a change, to save CI minutes.
- [x] **1.7 D-06 minimum.**
  - [x] `StyleManager::SetTheme` now updates `theme_` and logs `[T] StyleManager: Theme applied: <theme>`. This also fixes the stale Error palette: after the error panel sets `Error`, `ApplyCurrentTheme()` sees a difference and re-applies the configured theme.
  - [x] The change callback registers on `CONFIG_THEME` (`style_manager.theme`) instead of `"style_manager"`, so a theme change in Config applies to the styles at once.
  - [x] Wokwi case in part 3: after Day → Night the scenario expects `Theme applied: Night`, then repeats the menu path back to Day and expects `Theme applied: Day`. `release`, `test-wokwi` and `test-wokwi-release` build; part 3 passes (178 s, under the 5 minute cap). Only part 3 was run.
- [x] **1.8 D-18.** Fix the contradictory buffer-size comments. Choosing the line count is optional.
  - [x] Line count chosen deliberately: 60 lines (a quarter of the screen), sized as `SCREEN_WIDTH * BUFFER_LINE_COUNT * BYTES_PER_PIXEL` with 2 bytes for RGB565 instead of `sizeof(lv_color_t)`. The byte size is unchanged (28,800 B per buffer, ~57.6 KB for both), so there is no behaviour or RAM change. Both stale comments (`~38KB`, `~57KB`) now agree. `release` builds (1,339,183 B flash, RAM 35,356 B). No Wokwi run: no logic changed. The unused `display::BUFFER_LINE_COUNT` (40) and `display::LV_BUFFER_SIZE` in `constants.h`, which contradicted the real values, are removed.
- [x] **1.9 Interim safety items.**
  - [x] `IComponent` and `ISensor` have `virtual ~X() = default;` (D-17).
  - [x] `SplashPanel::GetAnimationTime()` parses with `std::from_chars` (D-23): a non-numeric or trailing-garbage value logs `Invalid splash duration '<v>', using default` and falls back to the default; the result is clamped to >= 0 so a too-small duration no longer gives a negative animation time. `release`, `test-wokwi` and `test-wokwi-release` build (`release` 1,339,291 B). Part 1 passes (boot through splash); no corrupt-value Wokwi case, since seeding one needs another test hook.

**Exit:** new Wokwi cases for D-03, D-07 and D-06 pass. D-05e is covered in Phase 6.

---

## Phase 2: Tooling

Goal: static feedback, and a host test target that Phase 3 can fill.

- [x] **2.1 Warnings (§7.1).** Add `build_src_flags` (`-Wall -Wextra -Wshadow -Wnon-virtual-dtor -Woverloaded-virtual -Wdouble-promotion -Wformat=2 -Wimplicit-fallthrough -Wnull-dereference`) so only project sources are affected. Fix what they find.
  - [x] Flags added to `esp32_base`. First `release` build gave 95 distinct warnings: 29 `-Wshadow` (constructor parameters renamed with an `In` suffix, three globals shadowed by parameters), 12 `-Wreorder` (initialiser lists reordered to declaration order), 8 `-Wdeprecated-enum-enum-conversion` (`LV_PART_x | LV_STATE_DEFAULT` now cast to `lv_style_selector_t`), 15 unused variables, 6 unused static press handlers in the Error, Oil and Splash panels (deleted, never referenced), 2 `-Wformat` (`int32_t` with `%d`), 1 `-Wdouble-promotion`. Log-only variables are `[[maybe_unused]]`.
  - [x] Warnings fired inside LovyanGFX headers (`-Wnon-virtual-dtor`, `-Woverloaded-virtual`) because `build_src_flags` also applies to library headers included from project code. The two `#include <LovyanGFX.hpp>` sites (`main.h`, `device_provider.h`) are wrapped in a diagnostic push/pop.
  - [x] `release`, `test-wokwi` and `test-wokwi-release` build with 0 warnings (`release` 1,339,231 B). `debug-upload` shows one pre-existing `CORE_DEBUG_LEVEL redefined` warning per file (`-D CORE_DEBUG_LEVEL=3` against the debug build type's own define), unrelated to the new flags and left for 2.3 or Phase 9. Wokwi parts 1 and 6 pass.
- [x] **2.2 Formatting and style (B-03, C-07).** Restore `.clang-format` (deleted in `d3d7308`); add `.editorconfig` and `.clang-tidy` (checks listed in §7.6, excluding `include/icons` and `lv_conf.h`).
  - [x] `.clang-format` restored unchanged from `d3d7308^` (Microsoft base, Allman, 4 spaces, 120 columns). `.editorconfig` added (UTF-8, LF, 4 spaces, final newline, trim trailing whitespace). `.clang-tidy` added with the §7.6 checks (`bugprone-easily-swappable-parameters` off as noise); `HeaderFilterRegex` is anchored to repo-relative project paths, so `include/icons`, `lv_conf.h` and `.pio/libdeps` are excluded. `compile_commands.json` is now git-ignored.
  - [x] Naming (decision: snake_case): `readability-identifier-naming` encodes `docs/standards.md`: PascalCase types and functions, snake_case variables, parameters and namespaces, snake_case members with a trailing `_` (static non-const members without it), UPPER_CASE constants. Ignored on purpose: `log_*` macros, `log_t_impl`, and the `Interrupts` namespace (the Arduino `interrupts` macro would collide).
  - [x] Whole tree formatted once with `clang-format` (85 files; include order now sorted, so `constants.h` got explicit `<cstddef>`/`<cstdint>` includes it used to get by accident). Then renamed with `clang-tidy --fix`, plus manual fixes where the tool missed uses: about 900 identifiers (member and local camelCase, `get_*` and `*_timer_callback` methods, `_animation_duration`, 41 PascalCase namespaces such as `Config` to `config`). String literals are untouched.
  - [x] Verified: all 7 envs build; `clang-tidy` naming check reports 0 warnings on `release` and `test-wokwi` (covering the `CLARITY_DEBUG` and test-only code); the set of `log_t` strings is identical to HEAD (77). Wokwi parts 1, 3 and 6 pass.
  - [x] Wokwi part 6 had been failing since `4f5c8e3` (never run): storage validation rejects a stored non-numeric splash duration (`Invalid stored value for splash_panel.duration`) before the splash panel's parser sees it, so the `Invalid splash duration 'abc'` line could not appear. `CLARITY_TEST_SEED_INVALID_SPLASH` no longer seeds NVS; it makes `SplashPanel::GetAnimationTime` use `abc` after the config query, which exercises the real `from_chars` fallback. Scenario order updated (default-panel line, then the splash line). Part 6 passes.
  - [x] clang-format and clang-tidy are not installed locally (pip copies in a scratch venv, with a compile database filtered of xtensa flags and `-Dfinal=` because clang rejects `final` on `GetShortPressFunction`). The CI format and lint jobs come in 2.6; 2.3 wires `pio check`.
- [x] **2.3 Static analysis.** Add `check_tool = cppcheck, clangtidy` and run `pio check`. Fix findings or add justified inline suppressions.
  - [x] `platformio.ini`: `check_tool = cppcheck`, `check_src_filters` (project sources and headers, no `include/icons` or `lv_conf.h`), `check_flags` for cppcheck (`warning,performance,portability`, inline suppressions). **clang-tidy is not a `check_tool`**: PlatformIO's own invocation passes only defines and includes (no `--target`, and its x86 clang-tidy 15 binary does not run on Apple Silicon at all), so it cannot parse the xtensa toolchain headers and reports false positives from half-parsed standard library types (`unique_ptr` members "uninitialised", unused `stringstream`, and so on). Instead `tools/run_clang_tidy.sh [env...]` builds a clang-friendly compile database per env (`tools/make_tidy_db.py`: drops the xtensa-only flags, adds `--target=xtensa`, the toolchain's C++ headers and `-Dfinal=` for the one declaration clang rejects) and runs the same `.clang-tidy` with warnings as errors, over `release`, `test-wokwi`, `test-wokwi-release`, `test-wokwi-splash` and `test-wokwi-failinit` so the `#ifdef` code is checked too. A deliberate bad name makes it exit 1.
  - [x] cppcheck findings fixed: `StorageProvider` initialises `config_mutex_` in the initialiser list, `ns.resize()` instead of `substr` to itself, and the constructor's call to `LoadAllConfigSections()` is qualified with an inline suppression (the class is never subclassed). `pio check -e release --fail-on-defect=low`: no defects.
  - [x] clang-tidy (23.1.4) findings fixed: 70 missing `override`, 39 missing braces, member and variable initialisation, unused parameters, two redundant member initialisers, narrowing casts, `MISO` and `BUZZER` macro parentheses, duplicated branches (`GetErrorColor`; the oil panel's initial animation start now has one `> scale_max` test instead of two branches that set the same value). Disabled with a reason in `.clang-tidy`: `bugprone-signed-bitwise` (LVGL flag macros are `int`), `performance-no-int-to-ptr` (LVGL animation values travel in `void*`), `cppcoreguidelines-special-member-functions` (the singleton, manager and panel classes go in phases 5 and 7), and four checks newer than 15 or pure style noise. Three `bugprone-branch-clone` spots carry a `NOLINTBEGIN`: their bodies are identical only when `log_w`/`log_t` compile out.
  - [x] One auto-fix was wrong and caught by Wokwi part 3: `performance-unnecessary-copy-initialization` turned `ConfigPanel::ShowOptionsMenu`'s `std::string key_str = full_key` into a reference, but callers pass a reference into `menu_items_`, which the function clears next, so the key dangled (the log printed `style_manager.theme:Day`). The copy is back with a `NOLINT` and the reason. It is the only auto-fix of that kind; the rest were checked against the diff. Parts 3, 4 and 5 pass again.
  - [x] A scan at `CORE_DEBUG_LEVEL=5` found 52 leftover camelCase names inside `log_*` arguments that the 2.2 rename never compiled (release builds drop those arguments), for example `animationStartValue` and `actionType`. All fixed; they would have broken a debug build at a higher log level. Names a misc-unused-parameters fix had commented out but a log still used are restored.
  - [x] All 7 envs build and the Wokwi suite passes (see the 2.6 note).
- [x] **2.4 `lib/core` skeleton.**
  - [x] `lib/core/library.json` and `lib/core/include/clarity/core/version.h` (`clarity::core::API_VERSION`).
  - [x] `[env:native]` (`platform = native`, `test_framework = unity` with Unity pinned to 2.6.1, `-Wall -Wextra -Werror -Wshadow`, `test_filter = native/*`).
  - [x] One trivial passing Unity test, `test/native/test_smoke`: `pio test -e native` passes.
- [x] **2.5 Layering check (§5.2).** `tools/check_layering.sh` fails if `lib/core` includes `Arduino.h`, LVGL, FreeRTOS, ESP-IDF, LovyanGFX, NVS or Arduino peripheral headers. Verified to exit 1 on a planted `Arduino.h` and `freertos/` include, and 0 on the clean tree.
- [x] **2.6 Add lint and `pio check` and `pio test -e native` jobs** to CI (T-04 steps 1 to 3).
  - [x] Final local verification on the committed code: all 7 envs build, `check_format.sh`, `check_layering.sh`, `pio check` (cppcheck), `run_clang_tidy.sh` and `pio test -e native` pass, and the full Wokwi suite passes 8 of 8 (72, 116, 141, 100, 107, 81, 55, 12 s).
  - [x] `.github/workflows/ci.yml`: `lint` (`tools/check_format.sh` with clang-format 23.1.5, then the layering check), `check` (`pio check` for cppcheck, then `tools/run_clang_tidy.sh` with clang-tidy 23.1.4) and `native-test` (`pio test -e native`), alongside the existing `build` matrix and `wokwi` job. `.github/workflows/README.md` and `docs/test.md` describe them. Seen green on GitHub (PR #56, run on `c673282`): `lint`, `check`, `native-test`, all 7 `build` jobs and the `wokwi` job (8 of 8 parts). The first `check` run failed because on Linux clang also searched the host glibc headers; `tools/make_tidy_db.py` now passes `-nostdlibinc` and adds the toolchain's GCC include directories.

**Exit:** `pio check` clean (or suppressions justified); native env runs in CI.

---

## Phase 3: Core extraction (not yet wired up)

Goal: the pure logic exists, with host tests, alongside the old code. Write the tests first (T-02).

- [x] **3.1 `types.h`:** `PanelId`, `ThemeId`, `KeyState`, `ErrorLevel`, `ButtonEvent`, `Fault` (enums only, with `ToString()`).
  - [x] `lib/core/include/clarity/core/types.h`: `PanelId` (Splash, Oil, Config, Error, Key, Lock), `ThemeId` (Day, Night, Error), `KeyState`, `ErrorLevel` (ordered by severity), `ButtonEvent` (None, Short, Long) and `Fault` (initial set from A-10, extended as later items need), each with a `constexpr ToString()` that returns `"Unknown"` for out-of-range values. PascalCase values per `docs/standards.md`; the old `definitions/enums.h` is untouched until phase 4 and 6. `test/native/test_types` written first: 8 tests; `pio test -e native` passes (9 cases), `check_layering.sh` OK. `clang-format` is not installed locally, so `check_format.sh` was not run; CI covers it.
- [ ] **3.2 `DebouncedInput` (A-03).** Stable-for-N-ms filter with `State()`/`Changed()` and seeding from the first raw read. Tests: bounce trains, stable change, `millis()` wrap (H-12).
- [ ] **3.3 `ButtonFsm` (A-03).** `Idle → Pressed → LongFired`; short on release in `[minShortMs, longPressMs)`; long fires once while held. Tests: below minimum, short, exactly at threshold, held past threshold (one event), bounce, release after long, threshold change mid-press.
- [ ] **3.4 `ErrorLog` (A-10).** Ring of 10; trim by severity then newest; 10 s warning expiry via `Expire(now)`; `DismissAt`; `Clear`; `Revision`. Tests per T-02.
- [ ] **3.5 `Units` (`unit_converter.h` → `units.h`).** `PressureUnit`/`TemperatureUnit` enums, rounding instead of truncation (C-03), no `double`. Tests for each conversion.
- [ ] **3.6 `ResolveDisplay` and `EffectiveTheme` (A-02).** Table-driven tests: the full scenario table in A-02, plus both key inputs active, error while lock active, boot with override. Resolve Q-05 (lights-off theme) before finalising `EffectiveTheme`.
- [ ] **3.7 `AnalogChannel` (A-04).** `LinearTransfer`, `Calibration` in engineering units, oversampling, IIR filter, plausibility. Transfer values are placeholders until Q-01 to Q-03 are answered (Phase 8).
- [ ] **3.8 `KeyValueStore` interface** and an in-memory fake for later `SettingsStore` tests.

**Exit:** about 60+ host tests green in CI. Nothing in `src/` references `lib/core` yet.

---

## Phase 4: Input and policy cut-over

Goal: replace the interrupt framework and `UIState` gate with sampled inputs, the pure policy and deferred transitions.

- [ ] **4.1 Make transitions deferred (D-13, A-05).**
  - [ ] Panels return a request instead of calling `PanelManager` directly; `PanelManager` applies it once per loop.
  - [ ] Swap order: load the new screen first, then delete the old panel (`CreateAndLoadPanelDirect`).
- [ ] **4.2 `Inputs` wrapper.** Own 5 `DebouncedInput` + 1 `ButtonFsm`; pins from the existing pin table; debug GPIO 34 behind `CLARITY_DEBUG` with 3-sample stability (H-09). Seed every input in `Begin()` before the first splash decision (A-03).
- [ ] **4.3 Wire the loop.** Replace `InterruptManager::Process()` with `Inputs::Sample` + `ResolveDisplay` + deferred apply. Preserve restoration, Config modality, key colour reload and splash deferral (Q-06) exactly as in §2.2.
- [ ] **4.4 Preserve test events.** Emit the same `[T]` strings (`CheckRestoration() called`, `No blocking interrupts - restoring to '…'`, `KeyPresentActivate()`, and so on).
- [ ] **4.5 Resolve Q-07 and Q-08.** Remove the BUSY wait (act immediately); decide splash short-press behaviour.
- [ ] **4.6 Delete:** `InterruptManager`, `TriggerHandler`, `ActionHandler`, `interrupts.h`, `Trigger`/`Action`, `Priority`/`TriggerType`, `UIState` and every `SetUiState`, `ErrorManager::SetErrorPanelActive`/`IsErrorPanelActive`, `GpioSensor`, `ButtonSensor`, `BaseSensor`, `ISensor`.
- [ ] **4.7 Fix frame pacing (D-12).** One `lv_timer_handler()` pump per loop, `lv_tick_set_cb(millis)`, sleep from the pump's return value. Delete `Ticker`.

**Exit:** scenario green; `grep SetUiState` empty; D-13 removed; one `lv_timer_handler` call site.

---

## Phase 5: UI layer

Goal: one panel model, one LVGL ownership rule, one theme.

- [ ] **5.1 `Panel` base (A-05)** with `Enter`/`Exit`/`Update(now)`/`OnButton` returning `NavIntent`. Document the four LVGL ownership rules in `ui/panel.h`.
- [ ] **5.2 `Theme` (A-08).** One `Palette` including all roles formerly in `Colors::`; `Apply(ThemeId)` is a no-op if unchanged, else `lv_obj_report_style_change(nullptr)`; `Active()` returns the applied theme. Fixes D-06 fully.
- [ ] **5.3 `ScreenManager`** with six long-lived panels, deferred `Apply()`, and the splash deferral.
- [ ] **5.4 Widgets (A-09).**
  - [ ] `Gauge` + `constexpr GaugeSpec` (moves dead-band and animation rules from `oem_oil_panel.cpp:293-367`; fixes D-08 and D-19 for gauges).
  - [ ] `IconView` for Key and Lock.
  - [ ] `MenuWheel` from `ConfigComponent`.
  - [ ] `ErrorView` that redraws only on log revision or index change.
- [ ] **5.5 Panels.**
  - [ ] Splash as a state machine (`FadeIn → Hold → FadeOut → Done`) driven by `Update(now)`; no `lv_timer`.
  - [ ] Oil, Key, Lock, Error (reading `ErrorLog`, no queue copy), Config.
- [ ] **5.6 Resolve Q-09 and Q-12** (Config Back target; error short press) and update the scenario.
- [ ] **5.7 Delete:** `IDisplayProvider`, `LvglDisplayProvider`, `IStyleManager`, `IPanelManager`, `IActionHandler`, `IComponent`, `IPanel`, `BasePanel`, old `*Component` classes, the old `ErrorManager`.

**Exit:** scenario green; Config theme round-trip test green; no `lv_timer` owned by a panel; no `lv_obj_del` outside `Panel::Exit()`.

---

## Phase 6: Settings

Goal: typed settings, compatible with existing NVS data, with every setting wired (D-05).

- [ ] **6.1 Resolve Q-04, Q-14, Q-15, Q-16** (debounce meaning, static menu table, migration policy, `system.update_rate`).
- [ ] **6.2 `Settings` struct and `SettingsStore` (A-07).**
  - [ ] Read existing namespaces, keys and types unchanged (§2.5), including `splash_panel.duration` as a string parsed with `std::from_chars`.
  - [ ] Validate on load; out-of-range or unknown values fall back to default and are rewritten.
  - [ ] `static_assert` that every namespace and key fits in 15 characters.
  - [ ] Add `cfg_meta/schema` and migration 1 per Q-15 and Q-16.
  - [ ] Native tests with the in-memory store, including a fixture built from a real device NVS dump.
- [ ] **6.3 `SettingsMenu` descriptor table.** Sorted sections, typed option lists, `DiagnosticPanel` absent. Native tests.
- [ ] **6.4 `NvsStore`.** Explicit `nvs_flash_init()`; erase only on `NO_FREE_PAGES`/`NEW_VERSION_FOUND` and log loudly (H-05).
- [ ] **6.5 Wire the dead settings (D-05).**
  - [ ] Brightness: `DisplayDriver::SetBrightness` (a).
  - [ ] Debounce per Q-04 (b).
  - [ ] Sensor offset/scale applied (c).
  - [ ] Per-sensor update rate (d).
  - [ ] Dead-band with a short NVS key (e).
  - [ ] Pass settings by value to consumers; use a `revision` counter, no callbacks.
- [ ] **6.6 Port `ConfigPanel`** to render the `SettingsMenu` table with typed intents; keep sorted sections, `Label: value` rows, toggles, `> ` marker, Exit.
- [ ] **6.7 Wokwi case:** saving the dead-band setting no longer raises an error (D-05e).
- [ ] **6.8 Delete:** `ConfigurationManager`, `StorageProvider`, `IConfigurationManager`, `IStorageProvider`, `IConfig`, `configs.h`, `ConfigConstants::*`, the six `AddSchema` blocks, `inline static ConfigItem` members.

**Exit:** an existing device's NVS values load identically; every menu setting takes effect.

---

## Phase 7: Composition root

Goal: no globals, no singletons, no factories. (restructure.md §11 notes this could move ahead of Phase 4 to avoid temporary global wiring; revisit when Phase 4 starts.)

- [ ] **7.1 `hal/board.h`** with `constexpr` pins, SPI, display geometry, ADC attenuation. Replace `gpio_pins.h`, `HardwareConstants` and the `device_provider.h` macros (D-16).
- [ ] **7.2 `hal/display_driver`** (from `DeviceProvider`): buffers via `heap_caps_malloc` with `width × lines × 2` bytes (D-18), `lv_display_flush_ready`, `lv_tick_set_cb`.
- [ ] **7.3 `hal/board_io`** (no interface): pin reads, `analogReadMilliVolts`, backlight PWM. ADC config once in `Begin()`.
- [ ] **7.4 `App` class (A-01, §5.4).** Owns everything by value in construction order; `Tick()` as in §5.4. State the single-task concurrency contract (C-09).
- [ ] **7.5 `main.cpp`** reduced to about 30 lines (`std::optional<App>`).
- [ ] **7.6 Fatal state (H-01).** Fault screen with code, `esp_reset_reason()` log, restart after 10 s, boot-loop guard. Replaces the Phase 1.5 guard.
- [ ] **7.7 Watchdog and breadcrumbs (H-02, H-03).** Enable the loop task watchdog; measure worst-case `Tick()` in debug; record reset reason and report `Fault::UnexpectedReset` after a panic or watchdog reset.
- [ ] **7.8 Delete:** `factories/*`, `main.h`, all `Instance()` accessors, setters, static self-registration lambdas, the remaining `include/interfaces/*` except `key_value_store.h`, `definitions/types.h`/`enums.h`/`state_types.h`/`ui_types.h`.
- [ ] **7.9 Programming errors become assertions** (§4.3 item 4); drop the 75 `ReportCriticalError` calls for impossible states and unreachable `if (!ptr)` checks (C-05).

**Exit:** `grep -R "Instance()" src` is empty; no non-`const` globals; scenario green.

---

## Phase 8: Sensing semantics

Goal: correct measurement, replacing placeholder transfer functions. Needs hardware and datasheets.

- [ ] **8.1 Resolve Q-01, Q-02, Q-03** (pressure scale and transfer, temperature sender characteristic, unit visibility).
- [ ] **8.2 Pressure.** Sender-specific `LinearTransfer`; value in bar as float; calibration in engineering units; gauge scale 0 to 6.0 bar per the decision.
- [ ] **8.3 Temperature.** Linear or NTC lookup table per Q-02.
- [ ] **8.4 ADC quality (H-07).** `analogReadMilliVolts`, oversampling, IIR filter, plausibility → `Fault::SensorImplausible` warning.
- [ ] **8.5 Presentation units (Q-03).** Numeric readout under each icon, or document the decision.
- [ ] **8.6 Tests.** Native tests for transfer, calibration, filter and plausibility. Bench-calibrate against known inputs on hardware.

**Exit:** bench readings match known inputs within tolerance.

---

## Phase 9: Assets, configuration and documentation

- [ ] **9.1 `lv_conf.h` (B-02).**
  - [ ] Choose the allocator deliberately (built-in TLSF pool, or C library with `LV_MEM_SIZE` removed).
  - [ ] Debug-only asserts, log and sysmon.
  - [ ] Disable unused widgets; keep `obj`, `label`, `image`, `line`, `scale`.
  - [ ] Trim fonts to the sizes in use; confirm with the map file.
  - [ ] Set `LV_DEF_REFR_PERIOD` deliberately.
- [ ] **9.2 Icons (B-05).** Commit the conversion script (`tools/convert_icons.py`, remove `**/convert.py` from `.gitignore`); pre-scale the oil icons; use `LV_COLOR_FORMAT_A8`; move data to `.c` files with `LV_IMAGE_DECLARE`. Compare flash against the Phase 0 baseline.
- [ ] **9.3 Partitions (B-04, Q-13).** Add `ota_1`, or switch to a single factory partition; resolve the `coredump` partition (H-04).
- [ ] **9.4 Versioning (B-06, H-10).** Generate the version from `git describe`; log it at boot; add an About row in Config.
- [ ] **9.5 Logging policy (A-11, H-11).** Remove the ~135 hot-path `log_v` calls; route all Wokwi-visible strings through `util/test_events.h`; add a CI check that every scenario `wait-serial` string exists there (T-04).
- [ ] **9.6 Heap budget logging (T-05).** Log free heap, minimum free heap and largest block at boot and every 60 s in debug.
- [ ] **9.7 Rename build envs (optional, §7.1).** If done, update `wokwi*.toml`, the test script and `.vscode/tasks.json` in the same change.
- [ ] **9.8 Comments and style sweep (C-01, C-07, C-08).** Remove stale and history comments and commented-out code; apply naming; add the `clarity::` namespace.
- [ ] **9.9 Documentation (§10, X-01 to X-11).**
  - [ ] New `architecture.md` replacing `context.md`, `patterns.md`, `interrupts.md`, `sensor.md` and `diagrams/*`.
  - [ ] Fold `requirements.md` into a short behavioural spec.
  - [ ] Update `config.md`, `hardware.md`, `test.md`, `standards.md`, `scenarios.md`.
  - [ ] Merge `log_t.md` and `error.md` into the architecture doc.
  - [ ] Fix README: remove unbacked claims (30.7 KB, 40-60%, CI) or back them with a report; fix commands.
  - [ ] Complete `docs/decisions.md`.
  - [ ] Track any remaining items as GitHub issues, then delete this file.
- [ ] **9.10 Hardware notes (H-06, H-08, H-09).** Document the GPIO 3 backlight and UART0 conflict, the GPIO 34 pull-down requirement and 12 V input conditioning.

**Exit:** flash and RAM report reviewed; docs reviewed.

---

## Decision checklist

Resolve each before the phase that needs it.

| Needed by | Decisions |
|---|---|
| Phase 0 | Q-11 toolchain |
| Phase 3 | Q-05 lights-off theme |
| Phase 4 | Q-06 splash preemption, Q-07 BUSY wait, Q-08 splash short press |
| Phase 5 | Q-09 Back target, Q-10 DiagnosticPanel (done in Phase 1), Q-12 error short press |
| Phase 6 | Q-04 debounce meaning, Q-14 menu definition, Q-15 migration, Q-16 `system.update_rate` |
| Phase 8 | Q-01 pressure scale, Q-02 temperature sender, Q-03 unit readout |
| Phase 9 | Q-13 OTA |

## Alternative path (single developer)

After Phase 0 and Phase 1, rebuild on a branch against the restructure.md §2 behavioural spec with the Wokwi scenario as the acceptance test, instead of Phases 4 to 7 as incremental cut-overs that each need temporary glue (for example `ConfigPanel` is rewritten in Phase 5 and again in Phase 6). `main` stays working throughout. Phases 2, 3, 8 and 9 apply unchanged.
