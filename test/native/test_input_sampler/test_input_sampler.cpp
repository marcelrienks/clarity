#include <cstdint>

#include <unity.h>

#include "clarity/core/input_sampler.h"

using clarity::core::ButtonEvent;
using clarity::core::InputSampler;
using clarity::core::RawInputs;
using clarity::core::VehicleInputs;

void setUp()
{
}
void tearDown()
{
}

namespace
{
constexpr uint32_t VEHICLE_MS = InputSampler::VEHICLE_DEBOUNCE_MS;
constexpr uint32_t BUTTON_MS = InputSampler::BUTTON_DEBOUNCE_MS;

RawInputs With(bool kp, bool knp, bool lock, bool lights, bool button = false, bool debug = false)
{
    RawInputs r;
    r.key_present = kp;
    r.key_not_present = knp;
    r.lock = lock;
    r.lights = lights;
    r.button = button;
    r.debug_error = debug;
    return r;
}

void AssertVehicle(bool kp, bool knp, bool lock, bool lights, const VehicleInputs &v)
{
    TEST_ASSERT_EQUAL(kp, v.key_present);
    TEST_ASSERT_EQUAL(knp, v.key_not_present);
    TEST_ASSERT_EQUAL(lock, v.lock);
    TEST_ASSERT_EQUAL(lights, v.lights);
}
} // namespace

void test_all_inactive_by_default()
{
    InputSampler s;
    s.Begin(0, RawInputs{});
    AssertVehicle(false, false, false, false, s.Vehicle());
    TEST_ASSERT_EQUAL(ButtonEvent::None, s.Button());
    TEST_ASSERT_FALSE(s.DebugErrorEdge());
}

void test_lines_active_at_boot_are_seen_at_once()
{
    InputSampler s;
    s.Begin(0, With(false, false, true, false));
    AssertVehicle(false, false, true, false, s.Vehicle());

    InputSampler k;
    k.Begin(0, With(true, false, false, true));
    AssertVehicle(true, false, false, true, k.Vehicle());
}

void test_vehicle_change_needs_the_debounce_period()
{
    InputSampler s;
    s.Begin(0, RawInputs{});
    s.Sample(10, With(false, false, true, false));
    AssertVehicle(false, false, false, false, s.Vehicle());
    s.Sample(10 + VEHICLE_MS - 1, With(false, false, true, false));
    AssertVehicle(false, false, false, false, s.Vehicle());
    s.Sample(10 + VEHICLE_MS, With(false, false, true, false));
    AssertVehicle(false, false, true, false, s.Vehicle());
}

void test_vehicle_bounce_is_ignored()
{
    InputSampler s;
    s.Begin(0, RawInputs{});
    uint32_t t = 0;
    for (int i = 0; i < 20; ++i)
    {
        t += 10;
        s.Sample(t, With(false, false, false, (i % 2) == 0));
        AssertVehicle(false, false, false, false, s.Vehicle());
    }
}

void test_lines_are_independent()
{
    InputSampler s;
    s.Begin(0, RawInputs{});
    s.Sample(5, With(false, true, false, false));
    s.Sample(5 + VEHICLE_MS, With(false, true, false, true));
    AssertVehicle(false, true, false, false, s.Vehicle());
    s.Sample(5 + 2 * VEHICLE_MS, With(false, true, false, true));
    AssertVehicle(false, true, false, true, s.Vehicle());
}

void test_short_press_through_the_debounce()
{
    InputSampler s;
    s.Begin(0, RawInputs{});
    uint32_t t = 100;
    // Held 300 ms raw; the debounced press starts BUTTON_MS later
    for (; t <= 400; t += 10)
    {
        s.Sample(t, With(false, false, false, false, true));
        TEST_ASSERT_EQUAL(ButtonEvent::None, s.Button());
    }
    ButtonEvent seen = ButtonEvent::None;
    for (; t <= 600; t += 10)
    {
        s.Sample(t, RawInputs{});
        if (s.Button() != ButtonEvent::None)
        {
            seen = s.Button();
        }
    }
    TEST_ASSERT_EQUAL(ButtonEvent::Short, seen);
}

void test_press_shorter_than_debounce_gives_nothing()
{
    InputSampler s;
    s.Begin(0, RawInputs{});
    s.Sample(100, With(false, false, false, false, true));
    for (uint32_t t = 110; t < 100 + BUTTON_MS - 10; t += 10)
    {
        s.Sample(t, RawInputs{});
        TEST_ASSERT_EQUAL(ButtonEvent::None, s.Button());
    }
    for (uint32_t t = 100 + BUTTON_MS; t < 2000; t += 10)
    {
        s.Sample(t, RawInputs{});
        TEST_ASSERT_EQUAL(ButtonEvent::None, s.Button());
    }
}

void test_long_press_fires_once_while_held()
{
    InputSampler s;
    s.Begin(0, RawInputs{});
    int longs = 0;
    int others = 0;
    for (uint32_t t = 100; t < 4000; t += 10)
    {
        s.Sample(t, With(false, false, false, false, true));
        longs += s.Button() == ButtonEvent::Long;
        others += s.Button() == ButtonEvent::Short;
    }
    TEST_ASSERT_EQUAL(1, longs);
    TEST_ASSERT_EQUAL(0, others);
    for (uint32_t t = 4000; t < 4400; t += 10)
    {
        s.Sample(t, RawInputs{});
        TEST_ASSERT_EQUAL(ButtonEvent::None, s.Button());
    }
}

void test_button_thresholds_can_change()
{
    InputSampler s;
    s.Begin(0, RawInputs{});
    s.SetButtonThresholds(50, 500);
    int longs = 0;
    for (uint32_t t = 100; t < 1000; t += 10)
    {
        s.Sample(t, With(false, false, false, false, true));
        longs += s.Button() == ButtonEvent::Long;
    }
    TEST_ASSERT_EQUAL(1, longs);
}

void test_button_held_at_boot_is_a_press_not_an_event()
{
    InputSampler s;
    s.Begin(0, With(false, false, false, false, true));
    s.Sample(10, With(false, false, false, false, true));
    TEST_ASSERT_EQUAL(ButtonEvent::None, s.Button());
}

void test_debug_edge_needs_three_stable_samples()
{
    InputSampler s;
    s.Begin(0, RawInputs{});
    s.Sample(10, With(false, false, false, false, false, true));
    TEST_ASSERT_FALSE(s.DebugErrorEdge());
    s.Sample(20, With(false, false, false, false, false, true));
    TEST_ASSERT_FALSE(s.DebugErrorEdge());
    s.Sample(30, With(false, false, false, false, false, true));
    TEST_ASSERT_TRUE(s.DebugErrorEdge());
    s.Sample(40, With(false, false, false, false, false, true));
    TEST_ASSERT_FALSE(s.DebugErrorEdge());
}

void test_debug_floating_line_gives_no_edge()
{
    InputSampler s;
    s.Begin(0, RawInputs{});
    for (uint32_t t = 10; t < 2000; t += 10)
    {
        // High for two samples, low for one: never three in a row
        s.Sample(t, With(false, false, false, false, false, (t / 10) % 3 != 0));
        TEST_ASSERT_FALSE(s.DebugErrorEdge());
    }
}

void test_debug_high_at_boot_is_not_an_edge_until_released()
{
    InputSampler s;
    s.Begin(0, With(false, false, false, false, false, true));
    for (uint32_t t = 10; t < 100; t += 10)
    {
        s.Sample(t, With(false, false, false, false, false, true));
        TEST_ASSERT_FALSE(s.DebugErrorEdge());
    }
    s.Sample(100, RawInputs{});
    int edges = 0;
    for (uint32_t t = 110; t < 150; t += 10)
    {
        s.Sample(t, With(false, false, false, false, false, true));
        edges += s.DebugErrorEdge();
    }
    TEST_ASSERT_EQUAL(1, edges);
}

void test_debug_each_new_press_is_one_edge()
{
    InputSampler s;
    s.Begin(0, RawInputs{});
    int edges = 0;
    uint32_t t = 0;
    for (int press = 0; press < 3; ++press)
    {
        for (int i = 0; i < 6; ++i)
        {
            t += 10;
            s.Sample(t, With(false, false, false, false, false, true));
            edges += s.DebugErrorEdge();
        }
        t += 10;
        s.Sample(t, RawInputs{});
    }
    TEST_ASSERT_EQUAL(3, edges);
}

void test_vehicle_debounce_across_the_millis_wrap()
{
    InputSampler s;
    const uint32_t start = 0xFFFFFFF0u;
    s.Begin(start, RawInputs{});
    s.Sample(start + 5, With(false, false, true, false));
    s.Sample(start + 5 + VEHICLE_MS, With(false, false, true, false));
    AssertVehicle(false, false, true, false, s.Vehicle());
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_all_inactive_by_default);
    RUN_TEST(test_lines_active_at_boot_are_seen_at_once);
    RUN_TEST(test_vehicle_change_needs_the_debounce_period);
    RUN_TEST(test_vehicle_bounce_is_ignored);
    RUN_TEST(test_lines_are_independent);
    RUN_TEST(test_short_press_through_the_debounce);
    RUN_TEST(test_press_shorter_than_debounce_gives_nothing);
    RUN_TEST(test_long_press_fires_once_while_held);
    RUN_TEST(test_button_thresholds_can_change);
    RUN_TEST(test_button_held_at_boot_is_a_press_not_an_event);
    RUN_TEST(test_debug_edge_needs_three_stable_samples);
    RUN_TEST(test_debug_floating_line_gives_no_edge);
    RUN_TEST(test_debug_high_at_boot_is_not_an_edge_until_released);
    RUN_TEST(test_debug_each_new_press_is_one_edge);
    RUN_TEST(test_vehicle_debounce_across_the_millis_wrap);
    return UNITY_END();
}
