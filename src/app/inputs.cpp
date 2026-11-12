#include "app/inputs.h"

#include <Arduino.h>

#include "hardware/gpio_pins.h"
#include "utilities/logging.h"

void Inputs::Begin(uint32_t now)
{
    gpio_.PinMode(gpio_pins::KEY_PRESENT, INPUT_PULLDOWN);
    gpio_.PinMode(gpio_pins::KEY_NOT_PRESENT, INPUT_PULLDOWN);
    gpio_.PinMode(gpio_pins::LOCK, INPUT_PULLDOWN);
    gpio_.PinMode(gpio_pins::LIGHTS, INPUT_PULLDOWN);
    gpio_.PinMode(gpio_pins::INPUT_BUTTON, INPUT_PULLDOWN);
#ifdef CLARITY_DEBUG
    // Input-only pin without an internal pull: needs the external pull-down from hardware.md
    gpio_.PinMode(gpio_pins::DEBUG_ERROR, INPUT);
#endif

    sampler_.Begin(now, Read());

    [[maybe_unused]] const clarity::core::VehicleInputs v = sampler_.Vehicle();
    log_i("Inputs seeded: key_present=%d key_not_present=%d lock=%d lights=%d", v.key_present, v.key_not_present,
          v.lock, v.lights);
}

void Inputs::Sample(uint32_t now)
{
    sampler_.Sample(now, Read());
}

clarity::core::RawInputs Inputs::Read()
{
    clarity::core::RawInputs raw;
    raw.key_present = gpio_.DigitalRead(gpio_pins::KEY_PRESENT);
    raw.key_not_present = gpio_.DigitalRead(gpio_pins::KEY_NOT_PRESENT);
    raw.lock = gpio_.DigitalRead(gpio_pins::LOCK);
    raw.lights = gpio_.DigitalRead(gpio_pins::LIGHTS);
    raw.button = gpio_.DigitalRead(gpio_pins::INPUT_BUTTON);
#ifdef CLARITY_DEBUG
    raw.debug_error = gpio_.DigitalRead(gpio_pins::DEBUG_ERROR);
#endif
    return raw;
}
