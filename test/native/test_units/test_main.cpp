#include <cmath>
#include <cstdint>
#include <limits>

#include <unity.h>

#include "clarity/core/units.h"

using namespace clarity::core;

void setUp()
{
}
void tearDown()
{
}

// ---- rounding ----

void test_round_to_int32_rounds_half_away_from_zero()
{
    TEST_ASSERT_EQUAL_INT32(0, RoundToInt32(0.0f));
    TEST_ASSERT_EQUAL_INT32(1, RoundToInt32(0.5f));
    TEST_ASSERT_EQUAL_INT32(-1, RoundToInt32(-0.5f));
    TEST_ASSERT_EQUAL_INT32(1, RoundToInt32(1.4f));
    TEST_ASSERT_EQUAL_INT32(-1, RoundToInt32(-1.4f));
    TEST_ASSERT_EQUAL_INT32(2, RoundToInt32(1.6f));
    TEST_ASSERT_EQUAL_INT32(-2, RoundToInt32(-1.6f));
    TEST_ASSERT_EQUAL_INT32(3, RoundToInt32(2.5f));
}

void test_round_to_int32_is_safe_for_out_of_range_input()
{
    TEST_ASSERT_EQUAL_INT32(INT32_MAX, RoundToInt32(1e30f));
    TEST_ASSERT_EQUAL_INT32(INT32_MIN, RoundToInt32(-1e30f));
    TEST_ASSERT_EQUAL_INT32(INT32_MAX, RoundToInt32(std::numeric_limits<float>::infinity()));
    TEST_ASSERT_EQUAL_INT32(0, RoundToInt32(std::nanf("")));
}

// ---- pressure ----

void test_bar_to_psi()
{
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.0f, BarToPsi(0.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 14.5038f, BarToPsi(1.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 72.519f, BarToPsi(5.0f));
}

void test_bar_to_kpa()
{
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.0f, BarToKpa(0.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 100.0f, BarToKpa(1.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 450.0f, BarToKpa(4.5f));
}

void test_pressure_round_trips()
{
    for (float bar = 0.0f; bar <= 10.0f; bar += 0.5f)
    {
        TEST_ASSERT_FLOAT_WITHIN(0.001f, bar, PsiToBar(BarToPsi(bar)));
        TEST_ASSERT_FLOAT_WITHIN(0.001f, bar, KpaToBar(BarToKpa(bar)));
    }
}

void test_convert_pressure_by_unit()
{
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 2.0f, ConvertPressure(2.0f, PressureUnit::Bar));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 29.0076f, ConvertPressure(2.0f, PressureUnit::Psi));
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 200.0f, ConvertPressure(2.0f, PressureUnit::Kpa));
}

void test_pressure_display_int_rounds_instead_of_truncating()
{
    // 1 bar = 14.5038 psi: truncation gives 14, rounding gives 15
    TEST_ASSERT_EQUAL_INT32(15, PressureToDisplayInt(1.0f, PressureUnit::Psi));
    // 0.5 bar = 7.2519 psi
    TEST_ASSERT_EQUAL_INT32(7, PressureToDisplayInt(0.5f, PressureUnit::Psi));
    // 4.96 kPa-scale value 4.9996 bar = 499.96 kPa: truncation gives 499
    TEST_ASSERT_EQUAL_INT32(500, PressureToDisplayInt(4.9996f, PressureUnit::Kpa));
    // 2.6 bar displays as 3 bar
    TEST_ASSERT_EQUAL_INT32(3, PressureToDisplayInt(2.6f, PressureUnit::Bar));
    TEST_ASSERT_EQUAL_INT32(0, PressureToDisplayInt(0.0f, PressureUnit::Psi));
}

// ---- temperature ----

void test_celsius_to_fahrenheit()
{
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 32.0f, CelsiusToFahrenheit(0.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 212.0f, CelsiusToFahrenheit(100.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -40.0f, CelsiusToFahrenheit(-40.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 98.6f, CelsiusToFahrenheit(37.0f));
}

void test_fahrenheit_to_celsius()
{
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, FahrenheitToCelsius(32.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 100.0f, FahrenheitToCelsius(212.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -40.0f, FahrenheitToCelsius(-40.0f));
}

void test_temperature_round_trip()
{
    for (float c = -40.0f; c <= 150.0f; c += 7.5f)
    {
        TEST_ASSERT_FLOAT_WITHIN(0.001f, c, FahrenheitToCelsius(CelsiusToFahrenheit(c)));
    }
}

void test_convert_temperature_by_unit()
{
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 90.0f, ConvertTemperature(90.0f, TemperatureUnit::Celsius));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 194.0f, ConvertTemperature(90.0f, TemperatureUnit::Fahrenheit));
}

void test_temperature_display_int_rounds_instead_of_truncating()
{
    // 37.5 C = 99.5 F rounds to 100; truncation gives 99
    TEST_ASSERT_EQUAL_INT32(100, TemperatureToDisplayInt(37.5f, TemperatureUnit::Fahrenheit));
    // -0.6 C displays as -1, not 0
    TEST_ASSERT_EQUAL_INT32(-1, TemperatureToDisplayInt(-0.6f, TemperatureUnit::Celsius));
    TEST_ASSERT_EQUAL_INT32(90, TemperatureToDisplayInt(89.6f, TemperatureUnit::Celsius));
    TEST_ASSERT_EQUAL_INT32(212, TemperatureToDisplayInt(100.0f, TemperatureUnit::Fahrenheit));
}

// ---- names, as stored in NVS and shown in the menu ----

void test_unit_names()
{
    TEST_ASSERT_EQUAL_STRING("Bar", ToString(PressureUnit::Bar));
    TEST_ASSERT_EQUAL_STRING("PSI", ToString(PressureUnit::Psi));
    TEST_ASSERT_EQUAL_STRING("kPa", ToString(PressureUnit::Kpa));
    TEST_ASSERT_EQUAL_STRING("C", ToString(TemperatureUnit::Celsius));
    TEST_ASSERT_EQUAL_STRING("F", ToString(TemperatureUnit::Fahrenheit));
    TEST_ASSERT_EQUAL_STRING("Unknown", ToString(static_cast<PressureUnit>(99)));
    TEST_ASSERT_EQUAL_STRING("Unknown", ToString(static_cast<TemperatureUnit>(99)));
}

void test_parse_pressure_unit()
{
    PressureUnit u = PressureUnit::Bar;
    TEST_ASSERT_TRUE(ParseUnit("PSI", u));
    TEST_ASSERT_EQUAL(PressureUnit::Psi, u);
    TEST_ASSERT_TRUE(ParseUnit("kPa", u));
    TEST_ASSERT_EQUAL(PressureUnit::Kpa, u);
    TEST_ASSERT_TRUE(ParseUnit("Bar", u));
    TEST_ASSERT_EQUAL(PressureUnit::Bar, u);
}

void test_parse_pressure_unit_rejects_bad_input_and_leaves_output()
{
    PressureUnit u = PressureUnit::Psi;
    TEST_ASSERT_FALSE(ParseUnit("psi", u));
    TEST_ASSERT_FALSE(ParseUnit("", u));
    TEST_ASSERT_FALSE(ParseUnit("C", u));
    TEST_ASSERT_FALSE(ParseUnit(nullptr, u));
    TEST_ASSERT_EQUAL(PressureUnit::Psi, u);
}

void test_parse_temperature_unit()
{
    TemperatureUnit u = TemperatureUnit::Celsius;
    TEST_ASSERT_TRUE(ParseUnit("F", u));
    TEST_ASSERT_EQUAL(TemperatureUnit::Fahrenheit, u);
    TEST_ASSERT_TRUE(ParseUnit("C", u));
    TEST_ASSERT_EQUAL(TemperatureUnit::Celsius, u);
    TEST_ASSERT_FALSE(ParseUnit("K", u));
    TEST_ASSERT_FALSE(ParseUnit(nullptr, u));
    TEST_ASSERT_EQUAL(TemperatureUnit::Celsius, u);
}

void test_conversions_are_constexpr()
{
    static_assert(BarToKpa(1.0f) == 100.0f, "kPa is exact");
    static_assert(CelsiusToFahrenheit(0.0f) == 32.0f, "freezing point");
    static_assert(RoundToInt32(2.5f) == 3, "round half away from zero");
    TEST_PASS();
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_round_to_int32_rounds_half_away_from_zero);
    RUN_TEST(test_round_to_int32_is_safe_for_out_of_range_input);
    RUN_TEST(test_bar_to_psi);
    RUN_TEST(test_bar_to_kpa);
    RUN_TEST(test_pressure_round_trips);
    RUN_TEST(test_convert_pressure_by_unit);
    RUN_TEST(test_pressure_display_int_rounds_instead_of_truncating);
    RUN_TEST(test_celsius_to_fahrenheit);
    RUN_TEST(test_fahrenheit_to_celsius);
    RUN_TEST(test_temperature_round_trip);
    RUN_TEST(test_convert_temperature_by_unit);
    RUN_TEST(test_temperature_display_int_rounds_instead_of_truncating);
    RUN_TEST(test_unit_names);
    RUN_TEST(test_parse_pressure_unit);
    RUN_TEST(test_parse_pressure_unit_rejects_bad_input_and_leaves_output);
    RUN_TEST(test_parse_temperature_unit);
    RUN_TEST(test_conversions_are_constexpr);
    return UNITY_END();
}
