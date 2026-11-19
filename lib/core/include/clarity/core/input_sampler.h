#pragma once

#include <cstdint>

#include "clarity/core/button_fsm.h"
#include "clarity/core/debounced_input.h"
#include "clarity/core/display_policy.h"
#include "clarity/core/types.h"

namespace clarity::core
{
/// One raw read of every input line, as the hal sees it
struct RawInputs
{
    bool key_present = false;
    bool key_not_present = false;
    bool lock = false;
    bool lights = false;
    bool button = false;
    bool debug_error = false; ///< Leave false on builds without the debug line
};

/// Debounces the vehicle lines and the button and turns the button into press events.
/// Begin() seeds every input from its first read, so a line active at boot is seen at once.
/// The debug line is not debounced by time: it counts as a rising edge after `debug_stable_samples`
/// consecutive high samples (it floats without an external pull-down, H-09), and a line already high at
/// Begin() is not an edge.
class InputSampler
{
  public:
    static constexpr uint32_t VEHICLE_DEBOUNCE_MS = 30;
    static constexpr uint32_t BUTTON_DEBOUNCE_MS = 50;
    static constexpr uint32_t DEFAULT_MIN_SHORT_MS = 50;
    static constexpr uint32_t DEFAULT_LONG_PRESS_MS = 1500;
    static constexpr uint8_t DEBUG_STABLE_SAMPLES = 3;

    void Begin(uint32_t now, const RawInputs &raw) noexcept
    {
        SampleLines(now, raw);
        debug_high_ = raw.debug_error;
        debug_count_ = raw.debug_error ? DEBUG_STABLE_SAMPLES : 0;
    }

    void Sample(uint32_t now, const RawInputs &raw) noexcept
    {
        SampleLines(now, raw);
        button_event_ = button_fsm_.Update(now, button_.State());

        debug_edge_ = false;
        if (!raw.debug_error)
        {
            debug_count_ = 0;
            debug_high_ = false;
        }
        else if (debug_count_ < DEBUG_STABLE_SAMPLES && ++debug_count_ == DEBUG_STABLE_SAMPLES && !debug_high_)
        {
            debug_high_ = true;
            debug_edge_ = true;
        }
    }

    constexpr VehicleInputs Vehicle() const noexcept
    {
        return {key_present_.State(), key_not_present_.State(), lock_.State(), lights_.State()};
    }

    /// The event completed by the last Sample(); None on most passes
    constexpr ButtonEvent Button() const noexcept
    {
        return button_event_;
    }

    /// True for the one Sample() that saw a stable rising edge on the debug line
    constexpr bool DebugErrorEdge() const noexcept
    {
        return debug_edge_;
    }

    constexpr void SetButtonThresholds(uint32_t min_short_ms, uint32_t long_press_ms) noexcept
    {
        button_fsm_.SetThresholds(min_short_ms, long_press_ms);
    }

  private:
    void SampleLines(uint32_t now, const RawInputs &raw) noexcept
    {
        key_present_.Sample(now, raw.key_present);
        key_not_present_.Sample(now, raw.key_not_present);
        lock_.Sample(now, raw.lock);
        lights_.Sample(now, raw.lights);
        button_.Sample(now, raw.button);
    }

    DebouncedInput key_present_{VEHICLE_DEBOUNCE_MS};
    DebouncedInput key_not_present_{VEHICLE_DEBOUNCE_MS};
    DebouncedInput lock_{VEHICLE_DEBOUNCE_MS};
    DebouncedInput lights_{VEHICLE_DEBOUNCE_MS};
    DebouncedInput button_{BUTTON_DEBOUNCE_MS};
    ButtonFsm button_fsm_{DEFAULT_MIN_SHORT_MS, DEFAULT_LONG_PRESS_MS};
    ButtonEvent button_event_ = ButtonEvent::None;
    uint8_t debug_count_ = 0;
    bool debug_high_ = false;
    bool debug_edge_ = false;
};
} // namespace clarity::core
