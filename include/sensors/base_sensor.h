#pragma once

#include "definitions/constants.h"
#include "definitions/types.h"
#include "interfaces/i_sensor.h"
#include <Arduino.h>
#include <functional>

#include "esp32-hal-log.h"

/**
 * @class BaseSensor
 * @brief Base class providing consistent change detection and interrupt registration for all sensors
 *
 * @details This base class implements the change detection pattern required by
 * the coordinated interrupt system. All sensors must inherit from BaseSensor
 * to ensure consistent change detection behavior and prevent corruption from
 * multiple evaluations per cycle.
 *
 * @design_pattern Template method pattern for change detection
 * @change_detection Prevents corruption through single evaluation per cycle
 * @memory_safe Designed for ESP32 memory constraints with minimal overhead
 * @interrupt_support Provides helper methods for registering sensor interrupts
 *
 * @architecture_requirement All sensors in the system must inherit from BaseSensor
 * to provide consistent change detection patterns for the interrupt system.
 *
 * @usage_pattern:
 * 1. Sensor inherits from both ISensor and BaseSensor
 * 2. Sensor implements HasStateChanged() method using DetectChange template
 * 3. Sensor optionally registers interrupts using RegisterInterrupt methods
 * 4. Interrupt evaluation calls HasStateChanged() exactly once per cycle
 * 5. Change detection prevents corruption from multiple calls
 *
 * @prevention Eliminates change detection corruption that occurs when
 * HasStateChanged() is called multiple times per evaluation cycle.
 *
 * @example:
 * ```cpp
 * class KeyPresentSensor : public ISensor, public BaseSensor {
 * public:
 *     void Init() override {
 *         RegisterTrigger("key_present", Priority::IMPORTANT,
 *                                InterruptEffect::LOAD_PANEL, 200);
 *     }
 *     bool HasStateChanged() {
 *         bool current_state = std::get<bool>(GetReading());
 *         return DetectChange(current_state, previous_state_);
 *     }
 * private:
 *     bool previous_state_ = false;
 * };
 * ```
 */
class BaseSensor : public ISensor
{
  public:
    // ========== Constructors and Destructor ==========
    ~BaseSensor() override = default;

    // ========== Public Interface Methods ==========
    // ISensor interface methods (must be implemented by concrete sensors)
    void Init() override = 0;
    Reading GetReading() override = 0;

    virtual bool HasStateChanged() = 0;

    virtual void OnInterruptTriggered()
    {
    }

  protected:
    // ========== Protected Methods ==========
    template <typename T> bool DetectChange(T current_value, T &previous_value)
    {
        if (!initialized_)
        {
            previous_value = current_value;
            initialized_ = true;
            return false; // No change on first read
        }

        bool changed = (current_value != previous_value);
        previous_value = current_value;
        return changed;
    }

    bool ShouldUpdate(unsigned long &last_update_time, unsigned long update_interval_ms)
    {
        unsigned long current_time = millis();
        if (current_time - last_update_time >= update_interval_ms)
        {
            last_update_time = current_time;
            return true;
        }
        return false;
    }

    bool IsValidAdcReading(int32_t raw_value)
    {
        return raw_value >= 0 && raw_value <= ADC_MAX_VALUE;
    }

    // ========== Protected Constants ==========
    static constexpr int32_t ADC_MAX_VALUE = 4095; // 12-bit ADC
    static constexpr float SUPPLY_VOLTAGE = 3.3f;  // ESP32 3.3V supply

    // ========== Protected Data Members ==========
    bool initialized_ = false; ///< Initialization flag for first-read handling
};