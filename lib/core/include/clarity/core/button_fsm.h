#pragma once

#include <cstdint>

#include "clarity/core/types.h"

namespace clarity::core
{
/// Turns a debounced button level into press events: Idle -> Pressed -> LongFired.
/// A short press is emitted on release when min_short_ms <= held < long_press_ms.
/// A long press is emitted once, while the button is still held, when held reaches long_press_ms.
class ButtonFsm
{
  public:
    constexpr ButtonFsm(uint32_t min_short_ms, uint32_t long_press_ms) noexcept
        : min_short_ms_(min_short_ms), long_press_ms_(long_press_ms)
    {
    }

    /// Thresholds apply to the press in progress as well as to later presses
    constexpr void SetThresholds(uint32_t min_short_ms, uint32_t long_press_ms) noexcept
    {
        min_short_ms_ = min_short_ms;
        long_press_ms_ = long_press_ms;
    }

    /// Feed the debounced level on every loop pass. Returns the event completed by this sample, if any.
    constexpr ButtonEvent Update(uint32_t now, bool pressed) noexcept
    {
        switch (state_)
        {
        case State::Idle:
            if (pressed)
            {
                state_ = State::Pressed;
                pressed_at_ = now;
            }
            return ButtonEvent::None;

        case State::Pressed: {
            const uint32_t held = static_cast<uint32_t>(now - pressed_at_);
            if (held >= long_press_ms_)
            {
                state_ = pressed ? State::LongFired : State::Idle;
                return ButtonEvent::Long;
            }
            if (!pressed)
            {
                state_ = State::Idle;
                return held >= min_short_ms_ ? ButtonEvent::Short : ButtonEvent::None;
            }
            return ButtonEvent::None;
        }

        case State::LongFired:
            if (!pressed)
            {
                state_ = State::Idle;
            }
            return ButtonEvent::None;
        }
        return ButtonEvent::None;
    }

  private:
    enum class State : uint8_t
    {
        Idle,
        Pressed,
        LongFired
    };

    uint32_t min_short_ms_;
    uint32_t long_press_ms_;
    uint32_t pressed_at_ = 0;
    State state_ = State::Idle;
};
} // namespace clarity::core
