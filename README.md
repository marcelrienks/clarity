# Clarity

**Embedded display manager**

Digital gauge cluster for vehicles, architected to manage any display via GPIO sensor inputs.

_Partially vibe coded with claude a learning excercise._

[![Status Badge](https://img.shields.io/badge/status-Testing-blue.svg)](https://github.com/marcelrienks/clarity)
![License Badge](https://img.shields.io/badge/license-MIT-blue)

## 🌟 **System Highlights**

- **Professional Architecture**: MVP pattern with factory-based dependency injection, optimized for embedded systems
- **Sampled Inputs**: debounced inputs and a pure display policy decide which panel and theme to show
- **Production Optimized**: 30.7KB flash reduction, 40-60% performance improvement across critical paths
- **Modern UI**: LVGL-based interface designed for 240x240 round displays
- **Comprehensive Testing**: Automated CI/CD with Wokwi simulation and integration testing
- **Multiple Panels**: Oil monitoring, error handling, configuration, and trigger-driven displays

## 📚 **Complete Documentation**

### **Core Architecture**
- **[Architecture Overview](docs/architecture.md)** - High-level MVP pattern and system design
- **[Interrupt Architecture](docs/interrupts.md)** - The old trigger system, replaced by `Inputs` and `ScreenManager` (rewrite pending, todo 9)
- **[Sensor Architecture](docs/sensor.md)** - Single ownership model for the oil sensors (rewrite pending, todo 9)
- **[Development Patterns](docs/patterns.md)** - Design patterns, string memory management, and coding practices

### **Implementation Details**
- **[Hardware Configuration](docs/hardware.md)** - ESP32 setup, GPIO mapping, input system architecture, and button behaviors
- **[System Requirements](docs/requirements.md)** - Comprehensive specifications with actual optimization results (30.7KB saved)
- **[Configuration System](docs/config.md)** - Dynamic self-registering configuration with IConfig interface and alphabetical organization
- **[Error Handling](docs/error.md)** - Error management, reporting, and panel integration
- **[Performance Logging](docs/log_t.md)** - Custom timing measurement system for optimization

### **Development & Testing**
- **[Testing Guide](docs/test.md)** - Complete Wokwi simulation setup, GitHub Actions CI/CD, and debugging procedures
- **[Coding Standards](docs/standards.md)** - Naming conventions, file organization, and code quality guidelines
- **[Usage Scenarios](docs/scenarios.md)** - User workflows, panel behaviors, and system interactions
- **[TODO List](docs/todo.md)** - Remaining tasks and future improvements

### **System Diagrams**
- **[Architecture Overview](docs/diagrams/architecture-overview.md)** - Complete component relationships and dependencies
- **[Application Flow](docs/diagrams/application-flow.md)** - Startup sequence, runtime processing, and interrupt handling
- **[Interrupt Handling Flow](docs/diagrams/interrupt-handling-flow.md)** - Detailed trigger/action processing workflow

### **CI/CD & Workflows**
- **[GitHub Actions Setup](.github/workflows/README.md)** - Automated testing configuration with Wokwi integration

## 🚀 **Quick Start**

### **Build Commands**
```bash
# Development builds
pio run -e debug-local     # Fast compilation for local testing
pio run -e test-wokwi      # Clean integration testing build

# Hardware deployment
pio run -e debug-upload    # Debug build with display inversion
pio run -e release         # Optimized production build
```

### **Testing & Simulation**
```bash
# Automated integration testing (5 Wokwi scenario parts, needs WOKWI_CLI_TOKEN)
pio run -e test-wokwi
cd test/wokwi && ./wokwi_test_automation.sh
```

---

## 🎯 **System Architecture**

### **MVP Pattern Implementation**
- **Models**: Sensors (ADC data acquisition for oil pressure and temperature)
- **Views**: Components (LVGL UI rendering - display-only for interrupt-driven panels)
- **Presenters**: Panels (business logic orchestration and lifecycle management)

### **Input and Display Policy**
- **Inputs**: samples the key, lock, lights and button lines every loop pass, with debouncing and a short/long press state machine
- **ScreenManager**: the single owner of the screen. It resolves the panel to show (errors, then key, then lock) and the theme (lights on gives Night) with the pure functions in `lib/core`
- **Deferred Transitions**: panels return a navigation intent instead of switching screens; the screen manager applies at most one transition per loop pass, never from inside a panel or an LVGL callback
- **Restoration**: automatic return to the user panel (the default panel) when the overrides clear

### **Key Features**
- **Immediate Response**: buttons and overrides act in the pass they arrive, also while a panel animates
- **Host-Tested Core**: input filtering, button timing and display policy run under `pio test -e native`
- **Memory Optimized**: Static arrays, zero hot path allocations

### **Panel System**
- **Oil Panel**: Primary monitoring with dual gauges and continuous sensor readings
- **Key/Lock Panels**: Trigger-driven security status displays
- **Error Panel**: System error management with cycling and clearing
- **Config Panel**: Settings management with persistent storage
- **Splash Panel**: Startup animation with skip functionality

## 🛠️ **Hardware Configuration**

### **Target Hardware**
- **Microcontroller**: NodeMCU-32S (ESP32-WROOM-32, 4MB Flash, 320KB RAM)
- **Display**: Waveshare 1.28" Round LCD (GC9A01, 240x240, SPI interface)
- **Sensors**: Oil pressure/temperature (ADC), GPIO triggers (Key, Lock, Lights)
- **Input**: Single button navigation (GPIO 32) with short/long press detection

## 🧪 **Testing & Quality Assurance**

### **Automated CI/CD Pipeline**
- **Basic Hardware Test**: GPIO, sensors, timing validation (5 test phases)
- **Full System Test**: Complete integration with panels, triggers, themes (7 test phases)
- **Wokwi Simulation**: Hardware emulation with automated pass/fail validation

### **Test Coverage**
- **Hardware Integration**: GPIO interrupt handling, ADC readings, button debouncing
- **System Integration**: Panel switching, trigger priorities, error handling
- **Performance Testing**: Memory usage validation, timing benchmarks
- **Regression Testing**: Automated on every commit and pull request

## 📦 **Dependencies**

### **Core Libraries**
- **[Arduino Framework](https://github.com/espressif/arduino-esp32)** - ESP32 hardware abstraction (Arduino core 3.3.12 via `espressif32` platform 8.0.0, pinned in `platformio.ini`)
- **[LVGL 9.3.0](https://docs.lvgl.io/master/)** - Advanced graphics library optimized for embedded
- **[LovyanGFX 1.2.7](https://github.com/lovyan03/LovyanGFX)** - High-performance display driver
- **[Preferences 2.0.0](https://github.com/espressif/arduino-esp32/tree/master/libraries/Preferences)** - ESP32 NVS storage wrapper

### **Development Tools**
- **[PlatformIO](https://platformio.org/)** - Cross-platform build system
- **[Wokwi](https://wokwi.com/)** - ESP32 hardware simulation platform
- **[GitHub Actions](https://github.com/features/actions)** - Automated CI/CD pipeline

## 🏆 **Credits & Inspiration**

### **Commercial Inspiration**
- **[Rotarytronics](https://www.rotarytronics.com/)** - Professional automotive gauge products

### **Technical Foundations**
- **[Garage Tinkering](https://github.com/garagetinkering)** - ESP32 automotive applications
- **[fbiego](https://github.com/fbiego)** - LVGL implementation patterns

### **Development Contributions**
- **[Eugene Petersen](https://github.com/gino247)** - Code review and architectural feedback

## 📝 **Project Philosophy**

_**Note:** This project represents a comprehensive exploration of professional software architecture patterns applied to embedded systems. While deliberately over-engineered for simple gauge applications, it demonstrates sophisticated design patterns, extensive testing frameworks, and production-ready optimization techniques. The result is a highly maintainable, testable, and performant embedded system that serves as an excellent foundation for complex automotive instrumentation projects._
