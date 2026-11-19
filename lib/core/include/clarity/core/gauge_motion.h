#pragma once

#include <cstdint>

namespace clarity::core
{
/// Dead-band size in scale units: percent of the scale range, never below one unit
constexpr int32_t DeadbandUnits(int32_t scale_min, int32_t scale_max, int32_t percent) noexcept
{
    const int32_t units = (percent * (scale_max - scale_min)) / 100;
    return units < 1 ? 1 : units;
}

/// When a gauge needle moves and where each move starts (D-08, D-19). Holds no LVGL state, so it is host-testable.
///
/// Values are scale units. The gauge asks Next() for each new reading, runs the animation it describes, and calls
/// Settled() when the needle comes to rest. Each gauge owns one instance, so nothing is shared between gauges.
class GaugeMotion
{
  public:
    struct Step
    {
        bool animate; ///< False: the change is inside the dead-band, leave the needle alone
        int32_t from; ///< Animation start; meaningful only when animate is true
        int32_t to;   ///< The new target, clamped to the scale
    };

    constexpr GaugeMotion(int32_t scale_min, int32_t scale_max, int32_t deadband) noexcept
        : min_(scale_min), max_(scale_max), deadband_(deadband < 1 ? 1 : deadband)
    {
    }

    /// Decides what a new reading does. An animating step records its target as the dead-band reference;
    /// one inside the dead-band changes nothing. The first reading always animates, from the scale minimum
    /// (a gauge starting up), or from the maximum when the reading is above the scale.
    constexpr Step Next(int32_t value) noexcept
    {
        const int32_t target = Clamp(value);
        if (!has_target_)
        {
            has_target_ = true;
            target_ = target;
            return {true, value > max_ ? max_ : min_, target};
        }

        const int32_t change = target > target_ ? target - target_ : target_ - target;
        if (change < deadband_)
        {
            return {false, rest_, target_};
        }

        target_ = target;
        return {true, rest_, target};
    }

    /// The needle came to rest at a value (an animation finished)
    constexpr void Settled(int32_t at) noexcept
    {
        rest_ = Clamp(at);
    }

    /// The needle was set directly, with no animation: it rests there and later readings measure from it
    constexpr void Jump(int32_t to) noexcept
    {
        has_target_ = true;
        target_ = Clamp(to);
        rest_ = target_;
    }

    /// Forgets every reading, so the next one is a first reading again
    constexpr void Reset() noexcept
    {
        has_target_ = false;
        target_ = min_;
        rest_ = min_;
    }

    constexpr void SetDeadband(int32_t deadband) noexcept
    {
        deadband_ = deadband < 1 ? 1 : deadband;
    }

    constexpr int32_t Deadband() const noexcept
    {
        return deadband_;
    }

    constexpr bool HasTarget() const noexcept
    {
        return has_target_;
    }

    constexpr int32_t Rest() const noexcept
    {
        return rest_;
    }

    constexpr int32_t Clamp(int32_t value) const noexcept
    {
        return value < min_ ? min_ : (value > max_ ? max_ : value);
    }

  private:
    int32_t min_;
    int32_t max_;
    int32_t deadband_;
    bool has_target_ = false;
    int32_t target_ = 0; ///< Last animated-to value: the dead-band reference
    int32_t rest_ = 0;   ///< Where the needle is: the start of the next animation
};
} // namespace clarity::core
