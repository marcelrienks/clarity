# Clarity Testing Guide

This document provides comprehensive instructions for testing the Clarity digital gauge system using the Wokwi ESP32 simulator. The testing framework includes both automated integration testing and manual debugging capabilities.

## Testing Environment Overview

Clarity provides automated integration testing using the Wokwi ESP32 simulator with build environment flexibility for different development needs:

| Build Environment | Purpose | Log Level | Usage |
|-------------------|---------|-----------|-------|
| **test-wokwi** | Automated CI/CD validation | TEST_LOGS only | Clean integration testing |
| **debug-local** | Development debugging | Full verbose + TEST_LOGS | Local development with Wokwi |

### Automated Integration Testing
- ✅ **Ultra-clean log output** - ONLY `[T]` test messages (all other logs suppressed)
- ✅ **Push button automation** - Complete scenario automation without manual interaction
- ✅ **2-minute timeout** - Prevents hanging in CI environments
- ✅ **Duplicate suppression** - Intelligent log_t() spam prevention
- ✅ **Cross-platform support** - Works on WSL2, macOS, and Linux

### Development Testing Features
- ✅ **Full verbose logging** - All `[V]`, `[D]`, `[I]`, `[W]`, `[E]`, and `[T]` messages when using debug-local
- ✅ **Same automation scripts** - Use either build environment with same test scenarios
- ✅ **Performance tracking** - Custom `log_t()` messages with duplicate suppression

## 📋 Build Commands

**Important:** Always run PlatformIO build commands from the project root directory, not from test subdirectories.

### Clean Integration Testing:
```bash
# From project root: Build for integration testing (TEST_LOGS only)
cd /path/to/Clarity
pio run -e test-wokwi

# Then run automated integration tests
cd test/wokwi
./wokwi_test_automation.sh
```

### Development Testing:
```bash
# From project root: Build for development testing (verbose logging)
cd /path/to/Clarity
pio run -e debug-local

# Then run automated tests with full logging
cd test/wokwi
./wokwi_test_automation.sh
```

## Test Structure

### Testing Files (`test/wokwi/`)
```
test/wokwi/
├── diagram_automated.json        # Hardware configuration with push buttons
├── diagram.json                  # Alternative hardware configuration
├── scenario_automated_1..5_*.yaml # Automated test, split in 5 parts (each boots fresh, see below)
├── wokwi_test_automation.sh      # Cross-platform automated test runner
├── wokwi_automated.toml         # Automated testing configuration
└── wokwi.toml                   # Base testing configuration
```

### Scenario parts

Wokwi caps a session at 5 minutes of wall-clock time and the simulator runs slowly, so the scenario is split into five parts that each boot the firmware fresh: triggers (steps 1-8), errors and restoration (9-13, starts with a preamble that rebuilds the step 8 trigger state), and three config parts (theme, sections, exit). `wokwi_test_automation.sh` runs them in order and exits non-zero if any part fails or times out. Each part takes 60-100 s.

Timing notes from the repair (0.3):
- Leave at least 1000 ms after anything that rebuilds a panel or menu before the next button press. 200-300 ms there stalled the simulator until the 5 minute cap.
- `wait-serial` only sees output printed after its step starts, so put the waits right after the action that produces the line. Long presses fire while the button is still held, so wait before releasing.
- Poll logs from `gpio_sensor.cpp` are off by default (about 96% of serial output). Define `GPIO_POLL_LOGS` to see them.

### Build Environments
- **`test-wokwi`**: Clean integration testing (CORE_DEBUG_LEVEL=0 + TEST_LOGS)
- **`debug-local`**: Full debug testing (CORE_DEBUG_LEVEL=5 + TEST_LOGS)
- **`test-wokwi-release`**: Release-like build (no `CLARITY_DEBUG`, `-Os`) with `TEST_LOGS` and `CLARITY_TEST_INJECT_ERROR`. For defects that only show without `CLARITY_DEBUG` (D-03). No scenario uses it yet.

### Error injection (`CLARITY_TEST_INJECT_ERROR`)

Builds without `CLARITY_DEBUG` have no debug error trigger. With `CLARITY_TEST_INJECT_ERROR`, `utilities/test_inject.h` reports one `ERROR` ("Injected test error") on each rising edge of GPIO 34, the same pin as the debug error button (`btn2` in `diagram_automated.json`). It goes through `ErrorManager::ReportError`, so a scenario sees `[T] Error reported: ERROR` as with the debug button. Boot logs `[T] Test error injection armed`. The flag and `CLARITY_DEBUG` are mutually exclusive (compile error). Serial input is not an option: the backlight PWM owns GPIO 3 (UART0 RX).

### Hardware Configuration

The Wokwi simulation uses standardized hardware configuration:

- **ESP32 DevKit-C v4**: Main microcontroller
- **ILI9341 Display**: 240x240 simulation (note: actual target is round GC9A01)
- **Action Button (btn1)**: GPIO 32 - Main user input button
- **Debug Button (btn2)**: GPIO 34 - Debug error trigger activation
- **Push Button Triggers**: Full automation control
  - **trigger_btn1 (Blue)**: GPIO 25 - Key Present
  - **trigger_btn2 (Yellow)**: GPIO 26 - Key Not Present
  - **trigger_btn3 (Orange)**: GPIO 27 - Lock Engaged
  - **trigger_btn4 (Purple)**: GPIO 33 - Lights On
- **Sensor Simulation**: Analog inputs (used by scenario automation)
  - **GPIO 36**: Oil pressure sensor simulation
  - **GPIO 39**: Oil temperature sensor simulation

## Prerequisites

### 1. Build Environment Setup
Ensure you have PlatformIO installed and the project builds successfully:
```bash
# Build for integration testing (TEST_LOGS only)
pio run -e test-wokwi
ls .pio/build/test-wokwi/firmware.bin

# Build for development testing (verbose logs + TEST_LOGS)
pio run -e debug-local
ls .pio/build/debug-local/firmware.bin
```

### 2. Wokwi CLI Installation
Install the Wokwi CLI tool from one of these sources:

**Option 1: Direct Download (macOS)**
```bash
# Download and install to ~/bin/
curl -L https://github.com/wokwi/wokwi-cli/releases/latest/download/wokwi-cli-macos -o ~/bin/wokwi-cli
chmod +x ~/bin/wokwi-cli
```

**Option 2: Direct Download (Linux/WSL2)**
```bash
# Download and install to ~/bin/
curl -L https://github.com/wokwi/wokwi-cli/releases/latest/download/wokwi-cli-linuxstatic-x64 -o ~/bin/wokwi-cli
chmod +x ~/bin/wokwi-cli
```

**Option 3: NPM Installation**
```bash
npm install -g @wokwi/cli
```

**Option 4: Check existing installation**
```bash
# Test if already installed
wokwi-cli --version
```

## Running Tests

### Automated Integration Testing

The fully automated integration test with push button simulation:

```bash
# Navigate to test directory
cd test/wokwi

# Run the fully automated integration test
./wokwi_test_automation.sh
```

**Features:**
- Cross-platform detection (WSL2, macOS, Linux)
- Automatically finds wokwi-cli in common locations
- Push button automation for 100% hands-off testing
- Duplicate suppression to prevent log spam
- Comprehensive scenario testing with 20+ test steps

### Build Environment Comparison

#### Clean Integration Testing (test-wokwi)
**Expected Output (TEST_LOGS only):**
```
========================================
 Clarity Wokwi Test Automation
========================================

Platform: macos
Wokwi CLI: /Users/username/bin/wokwi-cli

[T] SplashPanel loaded successfully
[T] OemOilPanel loaded successfully
[T] LightsOnActivate() - Setting NIGHT theme
[T] LockPanel loaded successfully
[T] KeyPanel loaded successfully
[T] Key icon color set to RED (key not present)
[T] Temperature reading: 85°C (x25)    ← Duplicate suppression
[T] ErrorPanel loaded successfully
← ONLY test messages shown, clean CI/CD output
```

#### Development Testing (debug-local)
**Expected Output (Full Logging):**
```
========================================
 Clarity Wokwi Test Automation
========================================

[V] Verbose debug messages
[D] Debug information
[I] General information
[W] Warnings
[E] Errors
[T] Timing/performance messages ← Custom log_t() with suppression
← All log levels shown for debugging
```

## Test Phases and Expected Behavior

The integration test follows the complete scenario from scenarios.md covering all 20 steps:

### System Startup (Steps 1-2)
- **Duration**: ~15 seconds
- **Expected**: Splash panel animation → Oil panel with animated gauges
- **Key Serial Patterns**:
  ```
  [T] SplashPanel loaded successfully
  [T] OemOilPanel loaded successfully
  ```

### Theme and Panel System (Steps 3-8)
- **Duration**: ~20 seconds
- **Expected**: Night theme → Lock panel → Key panels → Priority restoration
- **Key Serial Patterns**:
  ```
  [T] LightsOnActivate() - Setting NIGHT theme
  [T] LockPanel loaded successfully
  [T] KeyPanel loaded successfully
  [T] Key icon color set to RED (key not present)
  [T] Key icon color set to GREEN (key present)
  ```

### Error System Integration (Steps 9-11)
- **Duration**: ~10 seconds
- **Expected**: Error panel → Navigate errors → Clear all errors → Restore
- **Key Serial Patterns**:
  ```
  [T] ErrorPanel loaded successfully
  [T] ShortPressActivate() - Executing short press action
  [T] LongPressActivate() - Executing long press action
  ```

### Complete System Restoration (Steps 12-14)
- **Duration**: ~8 seconds
- **Expected**: Return to Oil panel → Day theme → Config access
- **Key Serial Patterns**:
  ```
  [T] No blocking interrupts - restoring
  [T] OemOilPanel loaded successfully
  [T] LightsOffActivate() - Setting DAY theme
  ```

## Success Criteria

### ✅ Core Functionality
- [ ] All 6 panels display correctly (Splash, Oil, Key, Lock, Error, Config)
- [ ] All 5 interrupts function with correct priorities
- [ ] Theme switching works (Day/Night) 
- [ ] Button actions work (short vs long press)
- [ ] Panel loading logs appear correctly
- [ ] Icon color changes are logged

### ✅ Integration Points  
- [ ] Panel priorities: Key (CRITICAL) > Lock (IMPORTANT) > Oil (default)
- [ ] Trigger restoration logic works correctly
- [ ] Theme persists across panel switches
- [ ] Error system overrides all other panels
- [ ] Icon color changes without panel reloads
- [ ] All automated scenario steps execute in order

### ✅ Performance
- [ ] No memory leaks or crashes
- [ ] System remains responsive throughout test
- [ ] All test logs appear in correct sequence
- [ ] Consistent timing measurements

## log_t() Function Integration

Both testing environments support the custom `log_t()` function with different behaviors:

### TEST_LOGS Mode (test-wokwi environment)
```cpp
#include "utilities/logging.h"  // Include the logging header

log_t("Panel loaded successfully");        // Shows: [T] Panel loaded successfully
log_t("Operation took %d ms", duration);   // Shows: [T] Operation took 150 ms

// All other log levels (log_e, log_w, log_i, log_d, log_v) are suppressed
log_i("This will not appear");             // Suppressed in test-wokwi
log_d("This will not appear");             // Suppressed in test-wokwi
```

### Full Debug Mode (debug-local environment)
```cpp
log_t("Panel loaded successfully");        // Shows: [T] Panel loaded successfully
log_i("General information");              // Shows: [I] General information
log_d("Debug details");                    // Shows: [D] Debug details
// All log levels work normally + TEST_LOGS
```

### Duplicate Suppression Examples
```cpp
// Repetitive sensor readings
log_t("Temperature reading: 85°C");        // Shows: [T] Temperature reading: 85°C
log_t("Temperature reading: 85°C");        // Silent (duplicate #2)
// ... 23 more identical messages suppressed silently
log_t("Temperature reading: 85°C");        // Shows: [T] Temperature reading: 85°C (x25)
log_t("Temperature reading: 86°C");        // Shows: [T] (total x27)
                                          //        [T] Temperature reading: 86°C
```

### Expected Output Examples
**Integration Testing (test-wokwi - TEST_LOGS only):**
```
[T] SplashPanel loaded successfully
[T] OemOilPanel loaded successfully
[T] KeyPanel loaded successfully
[T] Key icon color set to GREEN (key present)
[T] Temperature reading: 85°C (x25)
[T] LockPanel loaded successfully
[T] ErrorPanel loaded successfully
```

**Development Testing (debug-local - Full Logging + TEST_LOGS):**
```
[V] Verbose debug messages
[D] Debug information
[I] General information
[W] Warnings
[E] Errors
[T] Timing/performance messages ← With duplicate suppression
```

### Benefits
- ⏱️ **Performance measurement** - Easy timing of critical operations
- 🔍 **Optimization tracking** - Before/after performance comparisons
- 🏷️ **Filtered logging** - Easy to grep/filter for timing data
- 📊 **Clean CI output** - Only essential test logs in automated testing
- 🧪 **Test assertions** - Perfect for automated test validation
- 🚫 **Duplicate suppression** - Prevents log spam from repetitive messages
- 🔧 **Build environment control** - Different logging per environment

## Troubleshooting

### Build Issues

**Problem**: Firmware build fails
```bash
# Clean and rebuild from project root
cd /path/to/Clarity
pio run -e debug-local --target clean
pio run -e debug-local

# Check for missing dependencies
pio lib list
```

**Problem**: Wrong build environment
```bash
# For integration testing (TEST_LOGS only)
pio run -e test-wokwi

# For development testing (verbose logs + TEST_LOGS)
pio run -e debug-local

# Available testing environments:
# - test-wokwi: Clean integration testing (CORE_DEBUG_LEVEL=0 + TEST_LOGS)
# - debug-local: Full debug testing (CORE_DEBUG_LEVEL=5 + TEST_LOGS)
```

**Problem**: PlatformIO command not found from test directory
```bash
# Always run from project root
cd /path/to/Clarity  # NOT from test/wokwi or test/manual
pio run -e test-wokwi
```

### Wokwi CLI Issues

**Problem**: wokwi-cli not found
```bash
# Check if installed
which wokwi-cli

# Install for macOS
curl -L https://github.com/wokwi/wokwi-cli/releases/latest/download/wokwi-cli-macos -o ~/bin/wokwi-cli
chmod +x ~/bin/wokwi-cli

# Install for Linux/WSL2
curl -L https://github.com/wokwi/wokwi-cli/releases/latest/download/wokwi-cli-linuxstatic-x64 -o ~/bin/wokwi-cli
chmod +x ~/bin/wokwi-cli

# Add to PATH if needed
echo 'export PATH="$HOME/bin:$PATH"' >> ~/.bashrc
source ~/.bashrc
```

**Problem**: Cross-platform path issues
The automation script automatically detects platform and finds wokwi-cli in common locations:
- macOS: `/Users/username/bin/wokwi-cli`, `/usr/local/bin/wokwi-cli`
- WSL2: `/home/username/bin/wokwi-cli`, `/usr/local/bin/wokwi-cli`
- Linux: `/home/username/bin/wokwi-cli`, `/usr/local/bin/wokwi-cli`

### Test Execution Issues

**Problem**: Test hangs waiting for interrupt callbacks
This is expected behavior in Wokwi simulation - GPIO changes are polled rather than generating actual interrupts. The TEST_LOGS_ONLY feature is still working correctly.

**Problem**: Only seeing [T] logs in integration test
This is the intended behavior! TEST_LOGS mode with test-wokwi environment suppresses all other log levels for clean CI/CD output.

**Problem**: Missing test phases  
- Check that all push buttons start in released position
- Verify automation scenario matches scenarios.md order
- Monitor for timeout issues

## Development Workflow

### For Feature Development
1. **Code Changes** → Build with `pio run -e debug-local` (from project root)
2. **Development Testing** → Use `cd test/wokwi && ./wokwi_test_automation.sh`
3. **Full Debug Output** → Verbose logging + automated scenarios for comprehensive debugging
4. **Performance Measurement** → Add `log_t()` calls with duplicate suppression
5. **Fix Issues** → Repeat until stable

### For CI/CD Integration
1. **Code Complete** → Build with `pio run -e test-wokwi` (from project root)
2. **Integration Testing** → Use `cd test/wokwi && ./wokwi_test_automation.sh`
3. **Automated Validation** → Clean output + automated pass/fail + `log_t()` timing
4. **Deploy** → If all tests pass

### Quick Commands Reference
```bash
# Development Testing (Full verbose output + TEST_LOGS)
cd /path/to/Clarity
pio run -e debug-local
cd test/wokwi
./wokwi_test_automation.sh

# Integration Testing (TEST_LOGS only output)
cd /path/to/Clarity
pio run -e test-wokwi
cd test/wokwi
./wokwi_test_automation.sh
```

## Integration with CI/CD

### GitHub Actions

The workflow is `.github/workflows/ci.yml` (see `.github/workflows/README.md`). It builds all four envs on every push and pull request, and runs the 5-part Wokwi scenario on non-draft pull requests from this repository. It needs the `WOKWI_CLI_TOKEN` secret and costs about 7 Wokwi CI minutes per run.

Wokwi CLI for Linux CI: `https://github.com/wokwi/wokwi-cli/releases/download/<tag>/wokwi-cli-linuxstatic-x64` (there is no `wokwi-cli-linux` asset).

This comprehensive testing framework provides automated integration testing with flexible build environments - supporting both clean CI/CD output and full debugging capabilities, with intelligent duplicate suppression to maintain readable logs during extended operation.