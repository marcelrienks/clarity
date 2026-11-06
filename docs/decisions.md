# Decisions

One line per resolved decision from [restructure.md](restructure.md) §12 (X-10).

| ID | Decision | Date |
|---|---|---|
| Q-11 | Stay on `platformio/platform-espressif32` 8.0.0 (Arduino core 3.3.12, IDF 5.5.5, GCC 14.2.0). It already gives the C++20 toolchain pioarduino was meant to provide, so no switch. LVGL 9.3.0 and LovyanGFX 1.2.7 build and link on it with the flag changes below. Pin the platform exactly in 0.4. | 2026-11-05 |

## Q-11 toolchain findings (task 0.1)

Source: `pio run -v` on PlatformIO Core 6.2.0, 2026-11-05.

| Item | Value |
|---|---|
| Platform | `espressif32` 8.0.0, from `platformio/platform-espressif32` (not pioarduino, not the 6.x the plan assumed) |
| Arduino core | `framework-arduinoespressif32` 4.30312.261024 (Arduino core 3.3.12) |
| ESP-IDF | v5.5.5 (`IDF_VER`) |
| Libs package | `framework-arduinoespressif32-esp32-libs` 3.3.12 |
| GCC | xtensa-esp-elf 14.2.0 (crosstool-NG esp-14.2.0_20260121) |
| C++ standard, effective | `gnu++2a` (C++20). `platformio.ini` sets `-std=gnu++17`, but the core's `cpp_flags` add `-std=gnu++2b` then `-std=gnu++2a` later on the command line, so the project flag is overridden. |
| Exceptions | `-fexceptions` on, no `-fno-exceptions` (C-06). RTTI off (`-fno-rtti`). |
| `lib/core` consequences | C++20 is available: `std::span`, designated initialisers, float `std::from_chars` (GCC 14). |

## Library compatibility (task 0.1)

The floating ranges in `platformio.ini` resolve today to LVGL 9.6.0, LovyanGFX 1.2.36, ArduinoJson 7.4.3 and Unity 2.6.1. With them, `release` does not build:

| LVGL | LovyanGFX | Result on this toolchain |
|---|---|---|
| 9.6.0 (floating) | 1.2.36 | Fails in `oem_oil_component.cpp`: `MAIN_DEFAULT` expands to `LV_PART_MAIN \| LV_STATE_DEFAULT`, which is `int`, passed as `lv_part_t` (`style_manager.h:15-17`). |
| 9.3.0 (pin, B-01) | 1.2.7 (pin, B-01) | Builds, with two flag changes below. |

Two flag changes are needed for any of the four envs to build on core 3.3.12:

1. Add `-D LV_KCONFIG_IGNORE`. Without it LVGL's `lv_blend_helium.S` includes `lv_conf_kconfig.h` → `esp_attr.h`, and the IDF include path is not passed to `.S` files. The project uses `lv_conf.h` through `LV_CONF_INCLUDE_SIMPLE`, so Kconfig handling is not needed.
2. Remove `-D ARDUINO_USB_MODE=1` and `-D ARDUINO_USB_CDC_ON_BOOT=1` (`release`, `debug-upload`). On a classic ESP32 they make `HardwareSerial.h` reference `HWCDCSerial`, which does not exist (`chip-debug-report.cpp`). 0.5 already lists these flags for removal.

Result with LVGL 9.3.0, LovyanGFX 1.2.7 and both changes (2026-11-06):

| Env | Build | RAM | Flash (of 2 MB app partition) |
|---|---|---|---|
| `release` | OK | 10.8% | 64.5% (1,351,723 B) |
| `debug-local` | OK | 10.9% | 69.8% (1,463,011 B) |
| `debug-upload` | OK | 10.8% | 69.6% (1,458,879 B) |
| `test-wokwi` | OK | 10.9% | 69.8% (1,463,011 B) |

These numbers are the size reference for 0.5. Both flag changes are in `platformio.ini` since 0.4. `debug-local` and `test-wokwi` grew by 252 B against the first measurement (2026-11-05, 1,462,759 B), after the 0.3 log changes (restored `ConfigPanel:` messages in `config_panel.cpp`; cause not confirmed). `release` and `debug-upload` are unchanged. Re-measured 2026-11-06 in the project directory, not a clean copy.

Note: while the VS Code PlatformIO Home processes are running, they can reinstall the floating library versions into `.pio/libdeps` mid-build. Verify builds from a copy of the project, or close VS Code first.
