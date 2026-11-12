#pragma once

#include <cstdint>

namespace clarity::core
{
/// Two-point linear map from sender millivolts to a value in base units (bar or degrees C).
/// The result is held inside [value_low, value_high], so it never extrapolates past the sender's range.
struct LinearTransfer
{
    float mv_low;
    float mv_high;
    float value_low;
    float value_high;

    constexpr float Apply(float mv) const noexcept
    {
        if (mv_high == mv_low)
        {
            return value_low;
        }
        float t = (mv - mv_low) / (mv_high - mv_low);
        t = t < 0.f ? 0.f : (t > 1.f ? 1.f : t);
        return value_low + t * (value_high - value_low);
    }
};

/// Applied to the transferred value, in engineering units (Q-01): value * scale + offset
struct Calibration
{
    float offset = 0.f;
    float scale = 1.f;
};

/// Raw input limits. A window mean outside [mv_min, mv_max] means an open or shorted sender.
struct PlausibleRange
{
    uint16_t mv_min = 0;
    uint16_t mv_max = UINT16_MAX;
};

/// One analog sender: oversample, convert, calibrate, smooth, and judge plausibility.
/// Sample() takes millivolts as often as the caller likes. Readings are averaged over each
/// sample_ms window (oversampling), then fed to a first-order IIR filter. The first accepted
/// window seeds the filter. An implausible window leaves Value() at the last good value and
/// clears Plausible() until a plausible window arrives. Window timing uses unsigned
/// subtraction, so it is correct across the 32-bit millisecond wrap.
class AnalogChannel
{
  public:
    constexpr AnalogChannel(LinearTransfer transfer, uint32_t sample_ms, PlausibleRange limits = {},
                            float filter_alpha = 0.25f) noexcept
        : transfer_(transfer), limits_(limits), sample_ms_(sample_ms), alpha_(filter_alpha)
    {
    }

    void Sample(uint32_t now, uint16_t millivolts) noexcept
    {
        updated_ = false;
        if (!started_)
        {
            started_ = true;
            window_start_ = now;
        }
        sum_ += millivolts;
        ++count_;
        if (static_cast<uint32_t>(now - window_start_) < sample_ms_)
        {
            return;
        }
        CloseWindow(now);
    }

    void SetCalibration(Calibration c) noexcept
    {
        calibration_ = c;
    }

    /// Filtered value in base units; 0 until the first plausible window
    constexpr float Value() const noexcept
    {
        return value_;
    }

    /// False after an implausible window (open or short circuit) until a plausible one arrives
    constexpr bool Plausible() const noexcept
    {
        return plausible_;
    }

    /// True once a plausible window has produced a value
    constexpr bool HasValue() const noexcept
    {
        return has_value_;
    }

    /// True only for the Sample() call that closed a window
    constexpr bool Updated() const noexcept
    {
        return updated_;
    }

    /// Mean millivolts of the last closed window, for diagnostics
    constexpr float LastMillivolts() const noexcept
    {
        return last_mv_;
    }

  private:
    void CloseWindow(uint32_t now) noexcept
    {
        const float mv = static_cast<float>(sum_) / static_cast<float>(count_);
        sum_ = 0;
        count_ = 0;
        window_start_ = now;
        updated_ = true;
        last_mv_ = mv;

        if (mv < static_cast<float>(limits_.mv_min) || mv > static_cast<float>(limits_.mv_max))
        {
            plausible_ = false;
            return;
        }
        plausible_ = true;
        const float x = transfer_.Apply(mv) * calibration_.scale + calibration_.offset;
        if (!has_value_)
        {
            has_value_ = true;
            value_ = x;
        }
        else
        {
            value_ += alpha_ * (x - value_);
        }
    }

    LinearTransfer transfer_;
    Calibration calibration_{};
    PlausibleRange limits_;
    uint32_t sample_ms_;
    float alpha_;
    uint32_t window_start_ = 0;
    uint32_t sum_ = 0;
    uint32_t count_ = 0;
    float value_ = 0.f;
    float last_mv_ = 0.f;
    bool started_ = false;
    bool has_value_ = false;
    bool plausible_ = true;
    bool updated_ = false;
};
} // namespace clarity::core
