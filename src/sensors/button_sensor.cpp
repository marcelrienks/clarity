#include "sensors/button_sensor.h"
#include "utilities/logging.h"
#include <Arduino.h>

#include "esp32-hal-log.h"

/**
 * @brief Constructor for ButtonSensor
 * @param gpioProvider GPIO abstraction provider
 *
 * Initializes the button sensor with GPIO provider following the
 * standard sensor pattern used throughout the system.
 */
ButtonSensor::ButtonSensor(IGpioProvider *gpio_provider) : gpio_provider_(gpio_provider)
{
    log_v("ButtonSensor() constructor called");
}

/**
 * @brief Initializes the button sensor hardware
 *
 * Configures GPIO 32 as input with internal pull-down resistor enabled.
 * This ensures the pin reads LOW when button is not pressed and HIGH
 * when pressed, providing reliable button state detection.
 */
void ButtonSensor::Init()
{
    log_v("ButtonSensor::Init() called");

    if (gpio_provider_)
    {
        gpio_provider_->PinMode(GPIO_PIN, INPUT_PULLDOWN);
        log_i("ButtonSensor initialized on GPIO %d", GPIO_PIN);
    }
    else
    {
        log_e("ButtonSensor::Init() - GPIO provider is null");
    }
}

/**
 * @brief Gets the current reading from the button sensor
 * @return Reading containing boolean value (true = pressed, false = released)
 *
 * Returns the current debounced button state. This method provides stable
 * button state readings by filtering electrical noise through debouncing.
 */
Reading ButtonSensor::GetReading()
{
    bool button_state = ReadDebouncedState();
    return button_state;
}

/**
 * @brief Detects if button state has changed
 * @return true if state changed since last check
 *
 * Uses BaseSensor's DetectChange pattern to track button state changes.
 * Provides consistent change detection across all sensor types.
 */
bool ButtonSensor::HasStateChanged()
{
    bool current_state = ReadDebouncedState();
    bool changed = DetectChange(current_state, last_reported_state_);

    if (changed)
    {
        log_i("ButtonSensor state changed to %s", current_state ? "PRESSED" : "RELEASED");
    }

    return changed;
}

/**
 * @brief Reads and debounces the raw button state
 * @return Debounced button state (true = pressed, false = released)
 *
 * Implements software debouncing to filter electrical noise and contact
 * bounce. Only updates the current button state after the raw signal
 * has been stable for at least DEBOUNCE_MS milliseconds.
 */
bool ButtonSensor::ReadDebouncedState()
{
    if (!gpio_provider_)
    {
        return false;
    }

    bool raw_state = gpio_provider_->DigitalRead(GPIO_PIN);
    unsigned long current_time = millis();

    // If the raw state has changed, reset the debounce timer
    if (raw_state != last_raw_state_)
    {
        last_debounce_time_ = current_time;
        last_raw_state_ = raw_state;
    }

    // Only update the current state if it's been stable for the debounce period
    if ((current_time - last_debounce_time_) > DEBOUNCE_MS)
    {
        if (raw_state != current_state_)
        {
            current_state_ = raw_state;
        }
    }

    return current_state_;
}