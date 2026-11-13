#pragma once

#include <cstdint>

/**
 * @file unit_converter.h
 * @brief Utility class for converting between different units of measurement
 *
 * Provides conversion functions for temperature and pressure units.
 * All sensors store values in base units (Celsius for temperature, Bar for pressure)
 * and use these functions to convert to display units as needed.
 */

namespace unit_converter
{

// ========== Pressure Conversions ==========
// Base unit: Bar

/**
 * @brief Convert pressure from Bar to PSI
 * @param bar Pressure in Bar
 * @return Pressure in PSI
 */
constexpr float BarToPsi(float bar)
{
    return bar * 14.5038f;
}

/**
 * @brief Convert pressure from Bar to kPa
 * @param bar Pressure in Bar
 * @return Pressure in kPa
 */
constexpr float BarToKpa(float bar)
{
    return bar * 100.0f;
}

/**
 * @brief Convert pressure from PSI to Bar
 * @param psi Pressure in PSI
 * @return Pressure in Bar
 */
constexpr float PsiToBar(float psi)
{
    return psi / 14.5038f;
}

/**
 * @brief Convert pressure from kPa to Bar
 * @param kpa Pressure in kPa
 * @return Pressure in Bar
 */
constexpr float KpaToBar(float kpa)
{
    return kpa / 100.0f;
}

// ========== Temperature Conversions ==========
// Base unit: Celsius

/**
 * @brief Convert temperature from Celsius to Fahrenheit
 * @param celsius Temperature in Celsius
 * @return Temperature in Fahrenheit
 */
constexpr float CelsiusToFahrenheit(float celsius)
{
    return (celsius * 9.0f / 5.0f) + 32.0f;
}

/**
 * @brief Convert temperature from Fahrenheit to Celsius
 * @param fahrenheit Temperature in Fahrenheit
 * @return Temperature in Celsius
 */
constexpr float FahrenheitToCelsius(float fahrenheit)
{
    return (fahrenheit - 32.0f) * 5.0f / 9.0f;
}

// ========== Integer Conversion Helpers ==========

/**
 * @brief Convert pressure from Bar to PSI (integer)
 * @param bar Pressure in Bar
 * @return Pressure in PSI (rounded)
 */
inline int32_t BarToPsiInt(int32_t bar)
{
    return static_cast<int32_t>(BarToPsi(static_cast<float>(bar)));
}

/**
 * @brief Convert pressure from Bar to kPa (integer)
 * @param bar Pressure in Bar
 * @return Pressure in kPa (rounded)
 */
inline int32_t BarToKpaInt(int32_t bar)
{
    return static_cast<int32_t>(BarToKpa(static_cast<float>(bar)));
}

/**
 * @brief Convert temperature from Celsius to Fahrenheit (integer)
 * @param celsius Temperature in Celsius
 * @return Temperature in Fahrenheit (rounded)
 */
inline int32_t CelsiusToFahrenheitInt(int32_t celsius)
{
    return static_cast<int32_t>(CelsiusToFahrenheit(static_cast<float>(celsius)));
}

} // namespace unit_converter