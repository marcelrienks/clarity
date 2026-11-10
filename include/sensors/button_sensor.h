#pragma once

#include "hardware/gpio_pins.h"
#include "interfaces/i_gpio_provider.h"
#include "interfaces/i_sensor.h"
#include "sensors/base_sensor.h"

/**
 * @class ButtonSensor
 * @brief Simple button state sensor with debouncing
 *
 * @details This sensor monitors the button (GPIO 32) and provides
 * debounced button state readings. It follows the standard sensor
 * pattern used by other sensors in the system (KeySensor, LockSensor, etc.)
 *
 * @gpio_configuration:
 * - Pin: GPIO 32 (general purpose I/O pin on ESP32)
 * - Mode: INPUT_PULLDOWN (internal pull-down resistor enabled)
 * - Logic: HIGH when pressed, LOW when not pressed
 *
 * @debouncing:
 * - 50ms debounce time to filter electrical noise
 * - Returns stable state after debounce period
 *
 * @consistency Follows same pattern as KeySensor, LockSensor, LightsSensor
 * @dependency_injection Uses IGpioProvider for hardware abstraction
 */
class ButtonSensor : public BaseSensor
{
  public:
    // ========== Constructors and Destructor ==========
    explicit ButtonSensor(IGpioProvider *gpio_provider);
    ~ButtonSensor() override = default;

    // ========== Public Interface Methods ==========
    // ISensor Interface Implementation
    void Init() override;
    Reading GetReading() override;

    // BaseSensor interface
    bool HasStateChanged() override;

    // ========== Public Constants ==========
    static constexpr uint32_t DEBOUNCE_MS = 50;
    static constexpr int GPIO_PIN = gpio_pins::INPUT_BUTTON;

  private:
    // ========== Private Methods ==========
    bool ReadDebouncedState();

    // ========== Private Data Members ==========
    IGpioProvider *gpio_provider_;

    // Debouncing state
    bool current_state_ = false;
    bool previous_state_ = false;
    bool last_raw_state_ = false;
    unsigned long last_debounce_time_ = 0;

    // For change detection (BaseSensor pattern)
    bool last_reported_state_ = false;
};