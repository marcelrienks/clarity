#pragma once

#include <cstdint>

namespace clarity::core
{
/// Reports a boolean line only after it has held one value for a stable period.
/// The first sample seeds the state directly, so a line that is active at boot is seen at once.
/// Elapsed time uses unsigned subtraction, so it is correct across the 32-bit millisecond wrap.
class DebouncedInput
{
  public:
    explicit constexpr DebouncedInput(uint32_t stable_ms) noexcept : stable_ms_(stable_ms)
    {
    }

    void Sample(uint32_t now, bool raw) noexcept
    {
        changed_ = false;
        if (!seeded_)
        {
            seeded_ = true;
            state_ = raw;
            candidate_ = raw;
            since_ = now;
            return;
        }
        if (raw != candidate_)
        {
            candidate_ = raw;
            since_ = now;
        }
        if (candidate_ != state_ && static_cast<uint32_t>(now - since_) >= stable_ms_)
        {
            state_ = candidate_;
            changed_ = true;
        }
    }

    /// The debounced value
    constexpr bool State() const noexcept
    {
        return state_;
    }

    /// True only for the Sample() call that accepted a new value
    constexpr bool Changed() const noexcept
    {
        return changed_;
    }

    constexpr bool Seeded() const noexcept
    {
        return seeded_;
    }

  private:
    uint32_t stable_ms_;
    uint32_t since_ = 0;
    bool seeded_ = false;
    bool state_ = false;
    bool candidate_ = false;
    bool changed_ = false;
};
} // namespace clarity::core
