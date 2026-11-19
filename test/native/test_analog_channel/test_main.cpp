#include <cstdint>

#include <unity.h>

#include "clarity/core/analog_channel.h"

using clarity::core::AnalogChannel;
using clarity::core::Calibration;
using clarity::core::LinearTransfer;
using clarity::core::PlausibleRange;

void setUp()
{
}
void tearDown()
{
}

// 500 mV to 2500 mV maps to 0 to 10 bar
static constexpr LinearTransfer kBar{500.f, 2500.f, 0.f, 10.f};
static constexpr PlausibleRange kLimits{200, 3000};

void test_transfer_endpoints_and_midpoint()
{
    TEST_ASSERT_EQUAL_FLOAT(0.f, kBar.Apply(500.f));
    TEST_ASSERT_EQUAL_FLOAT(10.f, kBar.Apply(2500.f));
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 5.f, kBar.Apply(1500.f));
}

void test_transfer_holds_inside_sender_range()
{
    TEST_ASSERT_EQUAL_FLOAT(0.f, kBar.Apply(100.f));
    TEST_ASSERT_EQUAL_FLOAT(10.f, kBar.Apply(3300.f));
}

void test_transfer_falling_characteristic()
{
    constexpr LinearTransfer falling{500.f, 2500.f, 120.f, 0.f};
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 60.f, falling.Apply(1500.f));
    TEST_ASSERT_EQUAL_FLOAT(120.f, falling.Apply(0.f));
}

void test_transfer_degenerate_span_returns_low_value()
{
    constexpr LinearTransfer flat{1000.f, 1000.f, 3.f, 9.f};
    TEST_ASSERT_EQUAL_FLOAT(3.f, flat.Apply(1000.f));
}

void test_no_value_before_first_window()
{
    AnalogChannel ch(kBar, 100, kLimits);
    ch.Sample(0, 1500);
    ch.Sample(50, 1500);
    TEST_ASSERT_FALSE(ch.HasValue());
    TEST_ASSERT_FALSE(ch.Updated());
    TEST_ASSERT_EQUAL_FLOAT(0.f, ch.Value());
}

void test_first_window_seeds_value_without_filtering()
{
    AnalogChannel ch(kBar, 100, kLimits);
    ch.Sample(0, 1500);
    ch.Sample(100, 1500);
    TEST_ASSERT_TRUE(ch.Updated());
    TEST_ASSERT_TRUE(ch.HasValue());
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 5.f, ch.Value());
}

void test_updated_only_on_the_closing_sample()
{
    AnalogChannel ch(kBar, 100, kLimits);
    ch.Sample(0, 1500);
    ch.Sample(100, 1500);
    TEST_ASSERT_TRUE(ch.Updated());
    ch.Sample(150, 1500);
    TEST_ASSERT_FALSE(ch.Updated());
}

void test_window_is_averaged()
{
    AnalogChannel ch(kBar, 100, kLimits);
    ch.Sample(0, 1000);
    ch.Sample(50, 2000);
    ch.Sample(100, 1500); // mean 1500 mV
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 1500.f, ch.LastMillivolts());
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 5.f, ch.Value());
}

void test_averaging_rejects_single_spike()
{
    AnalogChannel ch(kBar, 100, kLimits);
    for (uint32_t t = 0; t < 100; t += 10)
    {
        ch.Sample(t, 1500);
    }
    ch.Sample(100, 1500);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 5.f, ch.Value());
    // next window: one 2500 mV spike among ten 1500 mV samples, mean 1600 mV (5.5 bar), filtered to 5.125
    for (uint32_t t = 110; t < 200; t += 10)
    {
        ch.Sample(t, t == 150 ? 2500 : 1500);
    }
    ch.Sample(200, 1500);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 5.125f, ch.Value());
}

void test_iir_filter_moves_part_way_to_new_level()
{
    AnalogChannel ch(kBar, 100, kLimits, 0.5f);
    ch.Sample(0, 500);
    ch.Sample(100, 500);  // seeds 0 bar
    ch.Sample(200, 2500); // target 10 bar
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 5.f, ch.Value());
    ch.Sample(300, 2500);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 7.5f, ch.Value());
}

void test_iir_filter_converges_to_steady_input()
{
    AnalogChannel ch(kBar, 10, kLimits, 0.25f);
    ch.Sample(0, 500);
    ch.Sample(10, 500);
    for (uint32_t t = 20; t <= 500; t += 10)
    {
        ch.Sample(t, 2500);
    }
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 10.f, ch.Value());
}

void test_calibration_applies_in_engineering_units()
{
    AnalogChannel ch(kBar, 100, kLimits);
    ch.SetCalibration(Calibration{0.5f, 2.f}); // value * 2 + 0.5
    ch.Sample(0, 1500);
    ch.Sample(100, 1500); // 5 bar -> 10.5
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 10.5f, ch.Value());
}

void test_default_calibration_is_identity()
{
    AnalogChannel ch(kBar, 100, kLimits);
    ch.Sample(0, 1500);
    ch.Sample(100, 1500);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 5.f, ch.Value());
}

void test_plausible_by_default()
{
    AnalogChannel ch(kBar, 100, kLimits);
    TEST_ASSERT_TRUE(ch.Plausible());
}

void test_short_circuit_is_implausible_and_has_no_value()
{
    AnalogChannel ch(kBar, 100, kLimits);
    ch.Sample(0, 50);
    ch.Sample(100, 50);
    TEST_ASSERT_FALSE(ch.Plausible());
    TEST_ASSERT_FALSE(ch.HasValue());
    TEST_ASSERT_TRUE(ch.Updated());
}

void test_open_circuit_is_implausible()
{
    AnalogChannel ch(kBar, 100, kLimits);
    ch.Sample(0, 3300);
    ch.Sample(100, 3300);
    TEST_ASSERT_FALSE(ch.Plausible());
}

void test_limits_are_inclusive()
{
    AnalogChannel lo(kBar, 100, kLimits);
    lo.Sample(0, 200);
    lo.Sample(100, 200);
    TEST_ASSERT_TRUE(lo.Plausible());
    AnalogChannel hi(kBar, 100, kLimits);
    hi.Sample(0, 3000);
    hi.Sample(100, 3000);
    TEST_ASSERT_TRUE(hi.Plausible());
}

void test_implausible_window_holds_last_good_value_then_recovers()
{
    AnalogChannel ch(kBar, 100, kLimits, 1.f); // alpha 1: no smoothing
    ch.Sample(0, 1500);
    ch.Sample(100, 1500);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 5.f, ch.Value());
    ch.Sample(200, 3300);
    TEST_ASSERT_FALSE(ch.Plausible());
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 5.f, ch.Value());
    ch.Sample(300, 2500);
    TEST_ASSERT_TRUE(ch.Plausible());
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 10.f, ch.Value());
}

void test_implausible_window_does_not_enter_the_filter()
{
    AnalogChannel ch(kBar, 100, kLimits, 0.5f);
    ch.Sample(0, 1500);
    ch.Sample(100, 1500); // 5
    ch.Sample(200, 3300); // implausible, ignored
    ch.Sample(300, 1500);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 5.f, ch.Value());
}

void test_default_limits_accept_everything()
{
    AnalogChannel ch(kBar, 100);
    ch.Sample(0, 0);
    ch.Sample(100, 0);
    TEST_ASSERT_TRUE(ch.Plausible());
    ch.Sample(200, 65535);
    TEST_ASSERT_TRUE(ch.Plausible());
}

void test_window_close_across_the_ms_wrap()
{
    AnalogChannel ch(kBar, 100, kLimits);
    const uint32_t start = 0xFFFFFFC0u; // 64 ms before the wrap
    ch.Sample(start, 1500);
    ch.Sample(start + 50, 1500);
    TEST_ASSERT_FALSE(ch.Updated());
    ch.Sample(start + 100, 1500); // wraps to 36
    TEST_ASSERT_TRUE(ch.Updated());
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 5.f, ch.Value());
}

void test_zero_sample_period_closes_every_sample()
{
    AnalogChannel ch(kBar, 0, kLimits, 1.f);
    ch.Sample(0, 1500);
    TEST_ASSERT_TRUE(ch.Updated());
    ch.Sample(1, 2500);
    TEST_ASSERT_TRUE(ch.Updated());
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 10.f, ch.Value());
}

void test_constexpr_construction()
{
    constexpr AnalogChannel ch(kBar, 100, kLimits);
    static_assert(ch.Plausible(), "plausible by default");
    static_assert(!ch.HasValue(), "no value yet");
    TEST_ASSERT_TRUE(ch.Plausible());
}

int main(int, char **)
{
    UNITY_BEGIN();
    RUN_TEST(test_transfer_endpoints_and_midpoint);
    RUN_TEST(test_transfer_holds_inside_sender_range);
    RUN_TEST(test_transfer_falling_characteristic);
    RUN_TEST(test_transfer_degenerate_span_returns_low_value);
    RUN_TEST(test_no_value_before_first_window);
    RUN_TEST(test_first_window_seeds_value_without_filtering);
    RUN_TEST(test_updated_only_on_the_closing_sample);
    RUN_TEST(test_window_is_averaged);
    RUN_TEST(test_averaging_rejects_single_spike);
    RUN_TEST(test_iir_filter_moves_part_way_to_new_level);
    RUN_TEST(test_iir_filter_converges_to_steady_input);
    RUN_TEST(test_calibration_applies_in_engineering_units);
    RUN_TEST(test_default_calibration_is_identity);
    RUN_TEST(test_plausible_by_default);
    RUN_TEST(test_short_circuit_is_implausible_and_has_no_value);
    RUN_TEST(test_open_circuit_is_implausible);
    RUN_TEST(test_limits_are_inclusive);
    RUN_TEST(test_implausible_window_holds_last_good_value_then_recovers);
    RUN_TEST(test_implausible_window_does_not_enter_the_filter);
    RUN_TEST(test_default_limits_accept_everything);
    RUN_TEST(test_window_close_across_the_ms_wrap);
    RUN_TEST(test_zero_sample_period_closes_every_sample);
    RUN_TEST(test_constexpr_construction);
    return UNITY_END();
}
