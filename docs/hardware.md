# Hardware Documentation

**Related Documentation:**
- **[Architecture](architecture.md)** - Software architecture and GPIO sensor mapping
- **[Requirements](requirements.md)** - Hardware requirements and constraints
- **[Patterns](patterns.md)** - Development patterns and practices

## Overview

Clarity is designed for ESP32-based automotive gauge systems with a round display. This document details the intended hardware configuration.

## Target Hardware

### Microcontroller
- **Board**: NodeMCU-32S Development Board
- **Chip**: ESP32-WROOM-32 (4MB Flash)
- **Architecture**: Dual-core Tensilica Xtensa LX6
- **Clock Speed**: 240MHz
- **RAM**: 320KB total (250KB available after system overhead and OTA partitioning)

### Display
- **Model**: Waveshare Round 1.28" LCD Display Module
- **Controller**: GC9A01 
- **Resolution**: 240x240 pixels
- **Shape**: Round display
- **Interface**: SPI
- **Colors**: 65K colors (RGB565)

### Display Connections (SPI)
| Function | ESP32 Pin | Wire Color |
|----------|-----------|------------|
| SCK      | GPIO 18   | Orange     |
| MOSI     | GPIO 23   | Green      |
| MISO     | -1 (NC)   | -          |
| DC       | GPIO 16   | Blue       |
| CS       | GPIO 22   | Gold       |
| RST      | GPIO 4    | Brown      |
| BL       | GPIO 3    | Gray       |
| VCC      | 3.3V      | Purple     |
| GND      | GND       | White      |

## Sensor Inputs

### Analog Sensors (ADC)
| Sensor           | ESP32 Pin | ADC Channel | Range    | Purpose              |
|------------------|-----------|-------------|----------|----------------------|
| Oil Pressure     | GPIO 36   | VP (ADC1_0) | 0-4095   | Engine oil pressure  |
| Oil Temperature  | GPIO 39   | VN (ADC1_3) | 0-4095   | Engine oil temperature |

### Digital Sensors (GPIO)
| Sensor           | ESP32 Pin | Mode        | Pull Config | Purpose                    |
|------------------|-----------|-------------|-------------|----------------------------|
| Key Present      | GPIO 25   | INPUT       | PULLDOWN    | Ignition key detected      |
| Key Not Present  | GPIO 26   | INPUT       | PULLDOWN    | Ignition key not detected  |
| Lock State       | GPIO 27   | INPUT       | PULLDOWN    | Vehicle lock status        |
| Light Sensor     | GPIO 33   | INPUT       | PULLDOWN    | Day/night detection        |
| Input Button     | GPIO 32   | INPUT       | PULLDOWN    | Single button navigation   |
| Debug Error Sensor | GPIO 34 | INPUT       | EXTERNAL*   | Debug error trigger (dev only) |

*GPIO 34 is input-only without internal pulls. Requires external pull-down resistor for stable operation in debug builds only.

**Development Only**: Debug error sensor is only active in CLARITY_DEBUG builds. This feature is completely excluded from release builds to save memory and GPIO resources.

## Power Configuration
- **Supply Voltage**: 3.3V (regulated from 12V automotive supply)
- **Display Current**: ~40mA (typical)
- **ESP32 Current**: ~80mA (active), ~10µA (deep sleep)
- **Total System**: ~120mA (operational)

## Build Configurations

### Hardware-Specific Builds
- **debug-local**: Fast compilation for local development (no display inversion)
- **debug-upload**: Hardware upload build with display inversion for Waveshare
- **release**: Optimized production build with display inversion

### Conditional Compilation
```cpp
#ifdef WOKWI_EMULATOR
    // Enable horizontal mirroring for wokwi emulator limitation
    writeCommand(0x36);
    writeData(0x48);    // MX bit (0x40) + BGR bit (0x08)
#endif

#ifdef INVERT
    cfg.invert = true;  // For waveshare displays
#endif
```

## Hardware Assembly Notes

1. **SPI Connections**: Use short, twisted pair wires for clock/data lines
2. **Power Supply**: Ensure clean 3.3V supply with adequate current capacity
3. **ADC Inputs**: Add RC filter (10kΩ + 100nF) for sensor noise reduction
4. **Pull-up Resistors**: GPIO inputs require 10kΩ pull-up to 3.3V
5. **Automotive Integration**: Use automotive-grade connectors and shielded cables

## Input System Architecture

### Button Input Configuration
* **Hardware**: Push button wired to GPIO 32 (3.3V connection)
* **Detection**: Rising edge detection (trigger only when pin changes to HIGH)
* **Debouncing**: Hardware debouncing to prevent false triggers
* **Timing Detection**: ActionHandler manages press duration
  - **Short press**: 50ms - 2000ms (with debouncing)
  - **Long press**: 2000ms - 5000ms
  - **Maximum press**: 5000ms (auto-release)

### Input System Design
* **Architecture**: Input system integrated with ActionHandler for centralized button handling
* **Interface**: Panels implement IActionService interface for universal button functions
* **Timing Logic**: ActionHandler distinguishes short vs long press events
* **Universal Compatibility**: Button functions injected into Action interrupts for all panels

### Per-Panel Input Behaviors

#### Oil Panel
- **Short press**: No action
- **Long press**: Load config panel

#### Splash Panel
- **Short press**: Stop animation and load default panel immediately
- **Long press**: Load config panel

#### Error Panel
- **Short press**: Remove the viewed error and show the next; the last one clears all and restores the previous panel
- **Long press**: Clear all errors

#### Config Panel
- **Short press**: Cycle through options
- **Long press**: Set/edit highlighted option

### Configuration Panel Design

#### Visual Design
* ConfigPanel with styling similar to error component but with grey colors
* Simple menu structure suitable for single button navigation
* Multi-level navigation with clear visual feedback

#### Configuration Options

**Default Panel Selection:**
- Display list of all registered panels
- Short press: Cycle through available panels
- Long press: Set highlighted panel as default

**Splash Screen Toggle:**
- Enable/disable splash screen on startup
- Short press: Cycle between "Enabled" and "Disabled"
- Long press: Apply selected setting

**Exit Option:**
- Return to default panel
- Short press: Highlight exit option
- Long press: Execute exit and return to default panel

#### Persistence Integration
* Integrate with PreferenceManager for persistent storage of all settings
* Settings survive device reboots
* Settings validation on startup

### Technical Considerations

#### State Management
* Track current menu level and selected option
* Handle navigation state transitions
* Provide visual feedback for all interactions

#### Error Handling
* Graceful fallback if invalid states are reached
* Timeout mechanisms for unresponsive interactions
* Reset to safe state on errors