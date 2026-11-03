# TODO

## General

* Investigate espressif32 version in ini file

## Project Review Findings

Source: project review of patterns, structure and code quality against current embedded C++ / PlatformIO community practice. Grouped by severity.

### Critical

Note after all the items listed in this file, a new restructure.md file was created which now supersedes the below

### High

- [ ] **Restore CI workflow.** `.github/workflows/` only contains a `README.md`; no workflow file exists, yet the main README claims automated CI/CD on every commit. Add a workflow that runs `pio run` for all environments and the Wokwi tests.
- [ ] **Add unit tests.** `test/` only holds Wokwi scenario files. Add `test_framework = unity` and a `native` PlatformIO environment with mocks for `IGpioProvider`, `IStorageProvider` etc. Start with ConfigurationManager, `unit_converter`, ErrorManager and trigger priority logic.
- [ ] **Remove Unity from production `lib_deps`.** `throwtheswitch/Unity` is listed in the `release` and `debug-upload` environments but unused by the firmware. Keep it in test environments only.
- [ ] **Remove or justify `-ffast-math` in `release`.** It permits NaN/inf assumptions and reassociation, which is risky for sensor and unit-conversion code. Only keep it if a measured benefit exists.

### Medium

- [ ] **Pin platform and library versions.** `platform = espressif32` is unpinned (see the inline TODO about pioarduino) and libraries use caret ranges (`^9.2.0`), while the README states LVGL 9.3.0. Pin exact versions so firmware builds are reproducible and docs match the build.
- [ ] **Add static analysis and warning flags.** Add `-Wall -Wextra`, `check_tool = cppcheck, clangtidy`, a `.clang-format` and an `.editorconfig`; run them in CI.
- [ ] **Reduce singletons and global state.**
  - `ErrorManager::Instance()` and `InterruptManager` are Meyers singletons; `PanelManager` uses a file-static raw `instancePtr_`; `main.cpp` holds globals.
  - Inject an `IErrorManager` instead of calling `Instance()` in constructors.
  - Replace static-init self-registration lambdas (`static bool x_registered = []{...}()`) with an explicit registration list in `main`, removing static-init-order risk.
- [ ] **Remove dead and half-failing defensive code.**
  - Drop `if (!ptr)` checks after `std::make_unique` (unreachable; `new` aborts on ESP32).
  - Constructors that log and `return` on null dependencies leave a half-built object. Take required dependencies by reference, or fail fast.
- [ ] **Cut `std::string` heap use on hot paths.** ~269 `std::string` uses on a 320KB device, including config lookups in `oem_oil_panel.cpp` (around lines 775 and 886). Use enums for units, `std::string_view` / `const char*` constants, or fixed buffers to avoid heap fragmentation.
- [ ] **Split large files and de-duplicate sensors.**
  - `oem_oil_panel.cpp` (~1040 lines), `config_panel.cpp` (~869), `storage_provider.cpp` (~688).
  - `oil_pressure_sensor.cpp` and `oil_temperature_sensor.cpp` are near-copies; extract a shared base or template.
- [ ] **Remove stale history comments and legacy code.** Comments such as "(new architecture)", "Legacy function for backward compatibility during migration", "ActionManager include removed", "factory pattern eliminated". Delete the unused legacy `registerSystemConfiguration()` in `main.cpp` if nothing calls it.

### Low

- [ ] **Remove `-I test` and `-I lib` from the production include path** in `platformio.ini`.
- [ ] **Review frame pacing.** `ticker.cpp` uses `delay()`; acceptable (yields to FreeRTOS), but consider a dedicated LVGL task with a mutex or `vTaskDelayUntil`.
- [ ] **Verify exception usage.** 13 grep hits for `try`/`catch`/`throw`; confirm whether any are real code. Arduino-ESP32 builds with `-fno-exceptions`, so real `try`/`catch` would be dead.
- [ ] **Clean up build-flag semantics.** `debug-local` sets `WOKWI_EMULATOR`, blurring "local build" and "simulator build". Reconsider the `TEST_LOGS` vs `CORE_DEBUG_LEVEL` split.
- [ ] **Icon headers.** Four generated headers total ~295k lines. Confirm they are `const` in flash (e.g. `LV_ATTRIBUTE_LARGE_CONST`) and not copied to RAM. `.gitignore` excludes `**/convert.py`, so generation is not reproducible from the repo; commit the script or document the steps.
- [ ] **Back up stated metrics.** "30.7KB flash reduction" and "40-60% performance improvement" have no benchmark or script in the repo. Add one or soften the claims.
- [ ] **Reconsider over-engineering.** Dual factories, an interface per single implementation, and the Trigger/Action framework for ~6 inputs add cost; decide what to simplify.

### Suggested order

1. Restore CI workflow; build all environments.
2. Pin versions; remove Unity from release; remove or justify `-ffast-math`.
3. Add warning flags, `.clang-format`, and cppcheck / clang-tidy in CI.
4. Add `native` test env with Unity and mocks.
5. Replace singletons with injection; replace static self-registration with an explicit list.
6. De-duplicate oil sensors; replace `std::string` unit lookups with enums.
7. Prune stale migration comments and dead null checks.
