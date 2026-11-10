## Overview
Clarity is an ESP32-based automotive digital gauge system featuring a 1.28" round LCD display (240x240 GC9A01 driver). The system provides real-time engine monitoring, security status display, and system configuration through an advanced interrupt-driven architecture optimized for automotive environments.

## Core Architecture

### MVP (Model-View-Presenter) Pattern
- **Models (Sensors)**: Hardware abstraction with change detection for GPIO/ADC inputs
- **Views (Components)**: LVGL-based UI elements with no business logic
- **Presenters (Panels)**: Orchestrate components and handle lifecycle management

### Key Design Principles
- **Single Ownership**: Each GPIO has exactly one dedicated sensor instance
- **Memory Efficiency**: Optimized for ESP32's 250KB available RAM constraint
- **Interrupt Safety**: LVGL-compatible processing during UI idle states only
- **Priority-Based Execution**: CRITICAL > IMPORTANT > NORMAL panel switching

## System Components

### Panel System (6 Main Panels)
- **Splash Panel**: Startup animation with configurable duration
- **Oil Panel**: Primary monitoring with dual gauges (pressure/temperature)
- **Key Panel**: Security key status indicator (display-only)
- **Lock Panel**: Vehicle security status (display-only) 
- **Error Panel**: System error management with navigation
- **Config Panel**: Dynamic configuration system with menu generation

### Interrupt Architecture (Advanced Trigger/Action Separation)
- **TriggerHandler**: GPIO state monitoring (Key, Lock, Lights sensors) - processes during UI IDLE only
- **ActionHandler**: Button event processing (short 50ms-2000ms, long 2000ms-5000ms) - evaluates every loop
- **Priority Override**: Sophisticated blocking logic prevents unnecessary panel switches
- **Smart Restoration**: Automatic return to last user-driven panel when triggers deactivate

### Sensor Architecture
- **BaseSensor Pattern**: Templated change detection across all sensor types
- **Handler Ownership**: TriggerHandler owns GPIO sensors, ActionHandler owns button sensor
- **Panel Ownership**: Data panels create their own ADC sensors for continuous monitoring
- **Change Detection**: First-read initialization with atomic state updates

## Hardware Configuration

### Target Hardware
- **Microcontroller**: ESP32-WROOM-32 (NodeMCU-32S)
- **Display**: 1.28" round LCD (240x240 GC9A01 driver) via SPI2_HOST
- **Input**: Single button GPIO 32 with INPUT_PULLDOWN
- **Memory**: 4MB flash with custom OTA partitioning

### GPIO Mapping
- **GPIO 32**: Main action button (universal short/long press)
- **GPIO 25**: Key Present trigger (CRITICAL priority)
- **GPIO 26**: Key Not Present trigger (CRITICAL priority)
- **GPIO 27**: Lock Engaged trigger (IMPORTANT priority)
- **GPIO 33**: Lights On trigger (NORMAL priority - theme switching)
- **GPIO 34**: Debug error trigger (development builds only)
- **GPIO 36/39**: ADC sensors (oil pressure/temperature)

## Configuration System

### Dynamic Configuration Architecture
- **Self-Registering Components**: Any component can register configuration requirements
- **Automatic UI Generation**: ConfigPanel dynamically generates menus
- **Type-Safe Access**: Compile-time checking with runtime flexibility
- **Persistent Storage**: NVS-based configuration persistence across reboots

### Configuration Categories
- **Oil Sensors**: Units (C/F, PSI/Bar), update rates, calibration offsets/scales
- **Display**: Brightness. The Day/Night theme is not a setting: it follows the dash-light signal
- **Splash Screen**: Duration configuration (1500ms-2500ms)
- **System Settings**: Default panel selection, update rates

## Error Handling System

### Error Architecture
- **Global ErrorManager**: Singleton service for centralized error collection
- **Bounded Queue**: Maximum 10 errors with FIFO replacement
- **Priority Integration**: Error trigger with CRITICAL priority overrides all panels
- **Error Levels**: WARNING (auto-dismiss 10s), ERROR (user action), CRITICAL (immediate attention)

### Error Flow
1. Component reports error to ErrorManager
2. Error trigger evaluates true when errors pending
3. ErrorPanel loads with CRITICAL priority
4. User navigates/acknowledges errors
5. System restores to previous panel when errors cleared

## Performance Optimization

### Memory Management (30.7KB Flash Savings Achieved)
- **Error Handling Optimization**: 15KB saved (fixed-size buffers vs dynamic strings)
- **GPIO Hot Path**: 3KB saved (eliminated allocations in critical paths)
- **Component System**: 2.5KB saved (removed hot path logging)
- **Sensor Architecture**: 2.1KB saved (eliminated double reads)
- **Panel System**: 1.8KB saved (animation state optimization)

### Processing Efficiency
- **Static Arrays**: Prevent heap fragmentation on ESP32
- **Template-Based Change Detection**: Minimal overhead with type safety
- **Idle-Only Execution**: Triggers process only during UI idle for LVGL compatibility
- **Direct Singleton Calls**: Eliminate context pointers for memory efficiency

## Testing & Quality Assurance

### Automated Testing Framework
- **Wokwi ESP32 Simulation**: Hardware emulation with push button automation
- **Dual Build Environments**: 
  - `test-wokwi`: Clean integration testing (TEST_LOGS only)
  - `debug-local`: Full verbose development testing
- **CI/CD Pipeline**: GitHub Actions with automated pass/fail validation

### Test Coverage
- **20-Step Integration Scenario**: Complete system workflow validation
- **Hardware Integration**: GPIO interrupts, ADC readings, button debouncing
- **Performance Monitoring**: Custom `log_t()` timing measurements with duplicate suppression
- **Cross-Platform**: WSL2, macOS, Linux compatibility

## Development Environment

### Build System
- **PlatformIO**: Cross-platform build system with multiple environments
- **Custom Partition Scheme**: Optimized for OTA updates
- **Memory Constraints**: All decisions account for ESP32 limitations

### Build Environments
- **debug-local**: Fast compilation for local testing with verbose logging
- **test-wokwi**: Clean integration testing build
- **debug-upload**: Debug build with display inversion for waveshare displays
- **release**: Optimized production build

## Key Interfaces & Patterns

### Core Interfaces
- **IPanel**: Panel lifecycle (Init → Load → Update)
- **IActionService**: Universal button handling across all panels
- **ISensor**: Hardware abstraction with change detection
- **IHandler**: Unified interrupt processing (Triggers & Actions)

### Factory Pattern Architecture
- **ProviderFactory**: Creates hardware abstraction providers
- **ManagerFactory**: Creates system managers with dependency injection
- **PanelFactory/ComponentFactory**: Singleton factories for UI elements

## Coding Standards

### Naming Conventions
- **Classes**: PascalCase (`PanelManager`)
- **Functions**: PascalCase (`SetTheme()`)
- **Variables**: snake_case with trailing underscore for members (`panel_manager_`)
- **Constants**: ALL_CAPS (`DAY`, `NIGHT`)
- **Files**: snake_case (`panel_manager.cpp`)
- **Interfaces**: I prefix (`IPanel`)

### Documentation Philosophy
- **Comment WHY, not WHAT**: Explain reasoning and architectural decisions
- **Public API Documentation**: Required Doxygen-style comments
- **Implementation Comments**: Selective explanations for complex logic only
- **Clean Code**: Remove obvious, redundant, and development debris comments

## System Capabilities

### Real-Time Monitoring
- **Continuous Sensor Reading**: Oil pressure/temperature with configurable update rates
- **Change-Based Triggers**: GPIO state monitoring with priority-based panel switching
- **Theme Management**: Automatic day/night switching based on lights sensor

### User Interface
- **Smooth Animations**: LVGL-based gauges with continuous movement
- **Universal Button System**: Consistent short/long press behavior across all panels
- **Visual Feedback**: Clear status indicators and loading animations
- **Error Integration**: Non-intrusive error notifications with priority handling

### Automotive Integration
- **Security Monitoring**: Key presence and lock status detection
- **Engine Parameters**: Oil pressure and temperature monitoring with configurable units
- **System Diagnostics**: Comprehensive error reporting and system status

## Future Extensibility

### Planned Enhancements
- **Additional Sensors**: Expandable sensor architecture for new parameters
- **Network Connectivity**: Telemetry and mobile app integration
- **Advanced Configuration**: Plugin architecture and theme marketplace
- **OTA Updates**: Over-the-air firmware update mechanism

### Architecture Strengths
- **Component-Based Design**: Easy addition of new panels and sensors
- **Dynamic Configuration**: Self-registering component configuration
- **Memory Efficient**: Optimized patterns suitable for embedded constraints
- **Well-Documented**: Comprehensive documentation and testing framework

This summary provides the essential context for understanding Clarity's sophisticated architecture, from its interrupt-driven core to its automotive-focused feature set, enabling efficient development and maintenance of this embedded system.