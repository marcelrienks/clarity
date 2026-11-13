# Dynamic Configuration System

## Overview

The Clarity configuration system enables components to self-register their settings with automatic UI generation, validation, and persistence. All configuration is managed through the `PreferenceManager` and displayed via the `ConfigPanel`.

## Key Features

- **Component Self-Registration**: Components register configuration requirements during initialization
- **Alphabetical Menu Organization**: Sections automatically sorted alphabetically by display name
- **Type-Safe Access**: Template-based configuration queries with compile-time type safety
- **Automatic UI Generation**: ConfigPanel dynamically builds menus from registered configurations
- **Persistent Storage**: Settings saved to ESP32 NVS with sectioned organization
- **Live Updates**: Real-time configuration changes via callback system
- **IConfig Interface**: Standard pattern for components with configuration needs

## Configuration Menu Tree

The following tree shows all configuration sections as they appear in the ConfigPanel, sorted **alphabetically** by display name:

```
📋 Configuration Menu
├── 📊 Display
│   └── Brightness → 10%, 25%, 50%, 75%, 100% (default: 80%)
│
├── 🛢️ Oil Pressure Sensor
│   ├── Pressure Unit → PSI, Bar, kPa (default: Bar)
│   ├── Update Rate (ms) → 250, 500, 1000, 2000 (default: 500)
│   ├── Calibration Offset → -1.0 to 1.0 in steps (default: 0.0)
│   └── Calibration Scale → 0.9 to 1.1 in steps (default: 1.0)
│
├── 🌡️ Oil Temperature Sensor
│   ├── Temperature Unit → C, F (default: C)
│   ├── Update Rate (ms) → 250, 500, 1000, 2000 (default: 500)
│   ├── Calibration Offset → -5.0 to 5.0 in steps (default: 0.0)
│   └── Calibration Scale → 0.9 to 1.1 in steps (default: 1.0)
│
├── 🎬 Splash Screen
│   └── Duration → 1500ms, 1750ms, 2000ms, 2500ms (default: 1500ms)
│
├── ⚙️ System
│   ├── Default Panel → OemOilPanel, ConfigPanel (default: OemOilPanel)
│   ├── Update Rate → 100, 250, 500, 750, 1000, 1500, 2000 ms (default: 500)
│   └── Show Splash → true/false (default: true)
│
└── 🔙 Exit
```

**Note**: Configuration options within each section appear in the order they are declared in code, not alphabetically.

## Architecture

### IConfig Interface Pattern

Components with configuration implement the `IConfig` interface:

```cpp
// include/interfaces/i_config.h
class IConfig {
public:
    virtual ~IConfig() = default;
    virtual void RegisterConfig(IPreferenceService* preferenceService) = 0;
};
```

### Component Configuration Pattern

Components declare their configuration items as inline static members in their headers:

```cpp
// include/sensors/oil_pressure_sensor.h
class OilPressureSensor : public BaseSensor, public IConfig {
private:
    // Configuration items declared in header as inline static
    inline static Config::ConfigItem unitConfig_{
        ConfigConstants::Items::UNIT,
        UIStrings::ConfigLabels::PRESSURE_UNIT,
        std::string(ConfigConstants::Defaults::DEFAULT_PRESSURE_UNIT),
        Config::ConfigMetadata("PSI,Bar,kPa", Config::ConfigItemType::Selection)
    };

    inline static Config::ConfigItem updateRateConfig_{
        ConfigConstants::Items::UPDATE_RATE,
        UIStrings::ConfigLabels::UPDATE_RATE_MS,
        ConfigConstants::Defaults::DEFAULT_UPDATE_RATE,
        Config::ConfigMetadata("250,500,1000,2000", Config::ConfigItemType::Selection)
    };
    // ... more config items
};
```

### Registration Implementation

Components register their configuration sections in their `RegisterConfig` method:

```cpp
// src/sensors/oil_pressure_sensor.cpp
void OilPressureSensor::RegisterConfig(IPreferenceService* preferenceService) {
    if (!preferenceService) return;

    Config::ConfigSection section(
        ConfigConstants::Sections::OIL_PRESSURE_SENSOR,  // Component name
        CONFIG_SECTION,                                   // Section key
        ConfigConstants::SectionNames::OIL_PRESSURE_SENSOR // Display name
    );

    // Add items in desired order (not alphabetical)
    section.AddItem(unitConfig_);
    section.AddItem(updateRateConfig_);
    section.AddItem(offsetConfig_);
    section.AddItem(scaleConfig_);

    preferenceService->RegisterConfigSection(section);
}
```

### System Configuration

System-wide settings are registered in `main.cpp`:

```cpp
// include/main.h - Configuration items declared globally
inline Config::ConfigItem defaultPanelConfig(
    ConfigConstants::Items::DEFAULT_PANEL,
    UIStrings::ConfigLabels::DEFAULT_PANEL,
    std::string(ConfigConstants::Panels::OEM_OIL_PANEL),
    Config::ConfigMetadata("OemOilPanel,ConfigPanel",
                          Config::ConfigItemType::Selection)
);

// src/main.cpp - Registration function
void registerSystemConfiguration() {
    Config::ConfigSection section(
        ConfigConstants::Sections::SYSTEM,
        ConfigConstants::Sections::SYSTEM,
        ConfigConstants::Sections::SYSTEM  // "System" as display name
    );

    section.AddItem(defaultPanelConfig);
    section.AddItem(updateRateConfig);
    section.AddItem(showSplashConfig);

    preferenceManager->RegisterConfigSection(section);
}
```

## Configuration Types

### ConfigItem Structure
- **key**: Item identifier (e.g., "unit", "brightness")
- **displayName**: UI label (e.g., "Pressure Unit", "Brightness")
- **defaultValue**: ConfigValue variant (bool, int, float, string)
- **metadata**: ConfigMetadata with constraints and type info

### ConfigMetadata
- **constraints**: Value constraints (ranges, options)
- **unit**: Unit suffix for display (e.g., "ms", "%")
- **type**: ConfigItemType (Selection, Numeric, Boolean)

### Supported Value Types
- **Boolean**: Toggle switch (true/false)
- **Integer**: Numeric selection or range
- **Float**: Decimal values with range constraints
- **String**: Text or enum selection

## Type-Safe Access

Configuration values are accessed using template methods:

```cpp
// Query configuration with type safety
if (auto unit = preferenceService->QueryConfig<std::string>("oil_pressure.unit")) {
    SetTargetUnit(*unit);
}

// Update configuration
preferenceService->UpdateConfig("oil_pressure.unit", std::string("PSI"));
```

## Live Configuration Updates

Components can register callbacks for configuration changes:

```cpp
void OilPressureSensor::RegisterLiveUpdateCallbacks() {
    auto callback = [this](const std::string& fullKey,
                          const std::optional<Config::ConfigValue>& oldValue,
                          const Config::ConfigValue& newValue) {
        if (fullKey == CONFIG_UNIT) {
            if (auto newUnit = Config::ConfigValueHelper::GetValue<std::string>(newValue)) {
                SetTargetUnit(*newUnit);
            }
        }
    };

    configCallbackId_ = preferenceService_->RegisterChangeCallback("oil_pressure", callback);
}
```

## Storage Organization

Configuration stored in ESP32 NVS with sectioned namespaces:

```
NVS Storage Structure:
├── cfg_system           → System settings
├── cfg_oil_pressure     → Oil pressure sensor config
├── cfg_oil_temp         → Oil temperature sensor config
├── cfg_style            → Display settings (brightness). The theme is not stored: it follows the lights signal
└── cfg_splash           → Splash screen settings
```

## Adding New Configuration

### For a New Component

1. **Implement IConfig interface**:
```cpp
class MyComponent : public IConfig {
    // ... component code
    void RegisterConfig(IPreferenceService* preferenceService) override;
};
```

2. **Declare config items in header**:
```cpp
private:
    inline static Config::ConfigItem myConfig_{
        "my_key", "Display Label",
        defaultValue,
        Config::ConfigMetadata("constraints", Config::ConfigItemType::Selection)
    };
```

3. **Register in RegisterConfig method**:
```cpp
void MyComponent::RegisterConfig(IPreferenceService* preferenceService) {
    Config::ConfigSection section("MyComponent", "my_section", "My Display Name");
    section.AddItem(myConfig_);
    preferenceService->RegisterConfigSection(section);
}
```

### For System Settings

Add to `main.h` and register in `registerSystemConfiguration()` in `main.cpp`.

## Benefits

- **Automatic Alphabetical Ordering**: No manual `displayOrder` management needed
- **Self-Documenting**: Configuration structure visible in component headers
- **Type Safety**: Compile-time type checking for configuration access
- **Separation of Concerns**: Components manage their own configuration
- **Zero UI Code**: ConfigPanel automatically generates appropriate UI controls
- **Consistent Pattern**: All components follow same IConfig interface pattern
- **Live Updates**: Changes take effect immediately without restart
- **Persistent Storage**: Automatic NVS persistence with namespace isolation