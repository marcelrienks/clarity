#pragma once

#include <cstdint>
#include <cstring>
#include <initializer_list>

namespace clarity::core
{
/// Display units. Sensor values are always held in base units (bar, degrees Celsius);
/// conversion happens only when a value is presented.
enum class PressureUnit : uint8_t
{
    Bar,
    Psi,
    Kpa
};

enum class TemperatureUnit : uint8_t
{
    Celsius,
    Fahrenheit
};

constexpr float PSI_PER_BAR = 14.5038f;
constexpr float KPA_PER_BAR = 100.0f;

/// Rounds half away from zero. Saturates out-of-range input and maps NaN to 0.
constexpr int32_t RoundToInt32(float value) noexcept
{
    if (value != value)
    {
        return 0;
    }
    if (value >= 2147483520.0f) // largest float below 2^31
    {
        return INT32_MAX;
    }
    if (value <= -2147483648.0f)
    {
        return INT32_MIN;
    }
    return static_cast<int32_t>(value >= 0.0f ? value + 0.5f : value - 0.5f);
}

// ---- pressure ----

constexpr float BarToPsi(float bar) noexcept
{
    return bar * PSI_PER_BAR;
}

constexpr float BarToKpa(float bar) noexcept
{
    return bar * KPA_PER_BAR;
}

constexpr float PsiToBar(float psi) noexcept
{
    return psi / PSI_PER_BAR;
}

constexpr float KpaToBar(float kpa) noexcept
{
    return kpa / KPA_PER_BAR;
}

/// Bar to the given display unit
constexpr float ConvertPressure(float bar, PressureUnit unit) noexcept
{
    switch (unit)
    {
    case PressureUnit::Psi:
        return BarToPsi(bar);
    case PressureUnit::Kpa:
        return BarToKpa(bar);
    case PressureUnit::Bar:
        break;
    }
    return bar;
}

constexpr int32_t PressureToDisplayInt(float bar, PressureUnit unit) noexcept
{
    return RoundToInt32(ConvertPressure(bar, unit));
}

// ---- temperature ----

constexpr float CelsiusToFahrenheit(float celsius) noexcept
{
    return celsius * 9.0f / 5.0f + 32.0f;
}

constexpr float FahrenheitToCelsius(float fahrenheit) noexcept
{
    return (fahrenheit - 32.0f) * 5.0f / 9.0f;
}

/// Degrees Celsius to the given display unit
constexpr float ConvertTemperature(float celsius, TemperatureUnit unit) noexcept
{
    return unit == TemperatureUnit::Fahrenheit ? CelsiusToFahrenheit(celsius) : celsius;
}

constexpr int32_t TemperatureToDisplayInt(float celsius, TemperatureUnit unit) noexcept
{
    return RoundToInt32(ConvertTemperature(celsius, unit));
}

// ---- names: the strings stored in NVS and offered by the menu ----

constexpr const char *ToString(PressureUnit unit) noexcept
{
    switch (unit)
    {
    case PressureUnit::Bar:
        return "Bar";
    case PressureUnit::Psi:
        return "PSI";
    case PressureUnit::Kpa:
        return "kPa";
    }
    return "Unknown";
}

constexpr const char *ToString(TemperatureUnit unit) noexcept
{
    switch (unit)
    {
    case TemperatureUnit::Celsius:
        return "C";
    case TemperatureUnit::Fahrenheit:
        return "F";
    }
    return "Unknown";
}

/// Exact, case-sensitive match. On failure `out` is left unchanged.
inline bool ParseUnit(const char *name, PressureUnit &out) noexcept
{
    if (name == nullptr)
    {
        return false;
    }
    for (PressureUnit candidate : {PressureUnit::Bar, PressureUnit::Psi, PressureUnit::Kpa})
    {
        if (std::strcmp(name, ToString(candidate)) == 0)
        {
            out = candidate;
            return true;
        }
    }
    return false;
}

inline bool ParseUnit(const char *name, TemperatureUnit &out) noexcept
{
    if (name == nullptr)
    {
        return false;
    }
    for (TemperatureUnit candidate : {TemperatureUnit::Celsius, TemperatureUnit::Fahrenheit})
    {
        if (std::strcmp(name, ToString(candidate)) == 0)
        {
            out = candidate;
            return true;
        }
    }
    return false;
}
} // namespace clarity::core
