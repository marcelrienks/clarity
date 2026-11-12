#pragma once

#include <cstdint>

#include "clarity/core/input_sampler.h"
#include "interfaces/i_gpio_provider.h"

/**
 * @class Inputs
 * @brief Reads the input pins once per loop pass and feeds clarity::core::InputSampler (A-03)
 *
 * @details Nothing here uses interrupts or reports events itself: the loop calls Sample() once,
 * then reads Vehicle(), Button() and DebugErrorEdge(). Begin() seeds every input from its first
 * raw read, so a key or lock that is active at boot is seen at once and the "no splash" decision
 * (restructure.md 2.1) can use it. The debug error line (GPIO 34) is read only with CLARITY_DEBUG;
 * it has no internal pull (see InputSampler for the stability rule).
 */
class Inputs
{
  public:
    explicit Inputs(IGpioProvider &gpio) noexcept : gpio_(gpio)
    {
    }

    /// Configure the pins and seed every input from its first read
    void Begin(uint32_t now);

    /// Read the pins and advance the debouncers and the button state machine
    void Sample(uint32_t now);

    clarity::core::VehicleInputs Vehicle() const noexcept
    {
        return sampler_.Vehicle();
    }

    clarity::core::ButtonEvent Button() const noexcept
    {
        return sampler_.Button();
    }

    /// Always false without CLARITY_DEBUG
    bool DebugErrorEdge() const noexcept
    {
        return sampler_.DebugErrorEdge();
    }

    void SetButtonThresholds(uint32_t min_short_ms, uint32_t long_press_ms) noexcept
    {
        sampler_.SetButtonThresholds(min_short_ms, long_press_ms);
    }

  private:
    clarity::core::RawInputs Read();

    IGpioProvider &gpio_;
    clarity::core::InputSampler sampler_;
};
