#pragma once

#include "hardware/gpio_pins.h"
#include "interfaces/i_gpio_provider.h"
#include "interfaces/i_sensor.h"
#include "sensors/base_sensor.h"
#include "definitions/types.h"
#include <Arduino.h>

/**
 * @struct GpioSensorConfig
 * @brief Configuration structure for generic GPIO sensors
 *
 * @details Defines all necessary parameters for configuring a GPIO sensor,
 * enabling the consolidation of multiple similar sensor classes into a
 * single generic implementation.
 */
struct GpioSensorConfig {
    uint8_t pin;                    ///< GPIO pin number
    uint8_t pinMode;                ///< Pin mode (INPUT, INPUT_PULLUP, INPUT_PULLDOWN)
    const char* name;               ///< Human-readable sensor name for logging
    bool activeHigh;                ///< true if HIGH means active, false if LOW means active
    bool debugOnly;                 ///< true if sensor is only available in debug builds

    constexpr GpioSensorConfig(
        uint8_t pinIn,
        uint8_t pinModeIn,
        const char* nameIn,
        bool activeHighIn = true,
        bool debugOnlyIn = false)
        : pin(pinIn)
        , pinMode(pinModeIn)
        , name(nameIn)
        , activeHigh(activeHighIn)
        , debugOnly(debugOnlyIn) {}
};

/**
 * @class GpioSensor
 * @brief Generic GPIO sensor implementation for digital input monitoring
 *
 * @details This class consolidates the functionality of multiple specific GPIO
 * sensor classes (KeyPresentSensor, KeyNotPresentSensor, LockSensor, LightsSensor,
 * DebugErrorSensor) into a single configurable implementation. This reduces
 * code duplication and simplifies maintenance while preserving all functionality.
 *
 * @architecture_benefits:
 * - Eliminates 4+ individual sensor classes (~150+ lines of duplicate code)
 * - Configuration-driven approach enables easy addition of new sensors
 * - Consistent behavior across all GPIO sensors
 * - Simplified testing with fewer classes to mock
 * - Reduced memory footprint through code consolidation
 *
 * @usage_example:
 * ```cpp
 * // Define configuration
 * constexpr GpioSensorConfig KEY_PRESENT_CONFIG{
 *     gpio_pins::KEY_PRESENT, INPUT_PULLDOWN, "KeyPresent", true, false
 * };
 *
 * // Create sensor
 * GpioSensor keyPresentSensor(KEY_PRESENT_CONFIG, gpioProvider);
 * ```
 *
 * @change_detection Uses BaseSensor DetectChange template for consistency
 * @memory_safe Designed for ESP32 memory constraints with minimal overhead
 * @interrupt_support Compatible with existing interrupt architecture
 */
class GpioSensor : public BaseSensor
{
public:
    // ========== Constructors and Destructor ==========
    GpioSensor(const GpioSensorConfig& config, IGpioProvider* gpioProvider);
    ~GpioSensor();

    // ========== Public Interface Methods ==========
    // ISensor interface implementation
    void Init() override;
    Reading GetReading() override;

    // BaseSensor interface implementation
    bool HasStateChanged() override;

    bool GetState();

protected:
    // ========== Protected Methods ==========
    void OnInterruptTriggered() override;

private:
    // ========== Private Methods ==========
    bool readRawPinState();
    bool readLogicalState();

    // ========== Private Data Members ==========
    GpioSensorConfig config_;       ///< Sensor configuration
    IGpioProvider* gpioProvider_;   ///< GPIO hardware abstraction
    bool previousState_;            ///< Previous state for change detection
};

// Predefined sensor configurations for common use cases
namespace sensor_configs {
    /**
     * @brief Key present sensor configuration (GPIO 25)
     */
    constexpr GpioSensorConfig KEY_PRESENT{
        gpio_pins::KEY_PRESENT, INPUT_PULLDOWN, "KeyPresent", true, false
    };

    /**
     * @brief Key not present sensor configuration (GPIO 26)
     */
    constexpr GpioSensorConfig KEY_NOT_PRESENT{
        gpio_pins::KEY_NOT_PRESENT, INPUT_PULLDOWN, "KeyNotPresent", true, false
    };

    /**
     * @brief Lock sensor configuration (GPIO 27)
     */
    constexpr GpioSensorConfig LOCK{
        gpio_pins::LOCK, INPUT_PULLDOWN, "Lock", true, false
    };

    /**
     * @brief Lights sensor configuration (GPIO 33)
     */
    constexpr GpioSensorConfig LIGHTS{
        gpio_pins::LIGHTS, INPUT_PULLDOWN, "Lights", true, false
    };

    /**
     * @brief Debug error sensor configuration (GPIO 34, debug builds only)
     */
    constexpr GpioSensorConfig DEBUG_ERROR{
        gpio_pins::DEBUG_ERROR, INPUT, "DebugError", true, true
    };
}