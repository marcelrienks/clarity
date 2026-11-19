#include <unity.h>

#include "clarity/core/gauge_motion.h"

using namespace clarity::core;

void setUp()
{
}
void tearDown()
{
}

// ---- DeadbandUnits ----

void test_deadband_is_a_percent_of_the_range()
{
    TEST_ASSERT_EQUAL_INT32(3, DeadbandUnits(0, 120, 3));
    TEST_ASSERT_EQUAL_INT32(3, DeadbandUnits(0, 60, 5));
    TEST_ASSERT_EQUAL_INT32(6, DeadbandUnits(0, 120, 5));
}

void test_deadband_is_at_least_one_unit()
{
    TEST_ASSERT_EQUAL_INT32(1, DeadbandUnits(0, 60, 1)); // 0.6 units
    TEST_ASSERT_EQUAL_INT32(1, DeadbandUnits(0, 60, 3)); // 1.8 units, truncated
    TEST_ASSERT_EQUAL_INT32(1, DeadbandUnits(0, 60, 0));
}

void test_constructor_floors_the_deadband_at_one()
{
    TEST_ASSERT_EQUAL_INT32(1, GaugeMotion(0, 60, 0).Deadband());
    TEST_ASSERT_EQUAL_INT32(1, GaugeMotion(0, 60, -4).Deadband());
}

// ---- first reading ----

void test_first_reading_always_animates_from_the_minimum()
{
    GaugeMotion motion(0, 60, 3);
    const auto step = motion.Next(40);
    TEST_ASSERT_TRUE(step.animate);
    TEST_ASSERT_EQUAL_INT32(0, step.from);
    TEST_ASSERT_EQUAL_INT32(40, step.to);
}

void test_first_reading_at_the_minimum_still_animates()
{
    GaugeMotion motion(0, 60, 3);
    TEST_ASSERT_TRUE(motion.Next(0).animate);
}

void test_first_reading_above_the_scale_starts_at_the_maximum()
{
    GaugeMotion motion(0, 60, 3);
    const auto step = motion.Next(75);
    TEST_ASSERT_TRUE(step.animate);
    TEST_ASSERT_EQUAL_INT32(60, step.from);
    TEST_ASSERT_EQUAL_INT32(60, step.to);
}

void test_first_reading_below_the_scale_is_clamped_to_the_minimum()
{
    GaugeMotion motion(0, 60, 3);
    const auto step = motion.Next(-9);
    TEST_ASSERT_EQUAL_INT32(0, step.from);
    TEST_ASSERT_EQUAL_INT32(0, step.to);
}

void test_a_minimum_other_than_zero_is_the_start()
{
    GaugeMotion motion(20, 80, 3);
    TEST_ASSERT_EQUAL_INT32(20, motion.Next(50).from);
}

// ---- dead-band ----

void test_change_inside_the_deadband_does_not_animate()
{
    GaugeMotion motion(0, 120, 3);
    motion.Next(50);
    motion.Settled(50);
    TEST_ASSERT_FALSE(motion.Next(52).animate);
    TEST_ASSERT_FALSE(motion.Next(48).animate);
}

void test_change_of_exactly_the_deadband_animates()
{
    GaugeMotion motion(0, 120, 3);
    motion.Next(50);
    motion.Settled(50);
    const auto step = motion.Next(53);
    TEST_ASSERT_TRUE(step.animate);
    TEST_ASSERT_EQUAL_INT32(50, step.from);
    TEST_ASSERT_EQUAL_INT32(53, step.to);
}

void test_deadband_applies_in_both_directions()
{
    GaugeMotion motion(0, 120, 3);
    motion.Next(50);
    motion.Settled(50);
    const auto step = motion.Next(47);
    TEST_ASSERT_TRUE(step.animate);
    TEST_ASSERT_EQUAL_INT32(47, step.to);
}

void test_deadband_is_measured_from_the_last_animated_target()
{
    GaugeMotion motion(0, 120, 3);
    motion.Next(50);
    motion.Settled(50);
    TEST_ASSERT_FALSE(motion.Next(52).animate); // inside
    TEST_ASSERT_FALSE(motion.Next(52).animate); // still measured from 50
    TEST_ASSERT_TRUE(motion.Next(53).animate);
}

void test_skipped_reading_leaves_the_needle_start_alone()
{
    GaugeMotion motion(0, 120, 3);
    motion.Next(50);
    motion.Settled(50);
    motion.Next(51); // skipped
    const auto step = motion.Next(60);
    TEST_ASSERT_EQUAL_INT32(50, step.from); // where the needle is, not the skipped 51
}

void test_a_skipped_step_reports_the_resting_state()
{
    GaugeMotion motion(0, 120, 3);
    motion.Next(50);
    motion.Settled(50);
    const auto step = motion.Next(51);
    TEST_ASSERT_FALSE(step.animate);
    TEST_ASSERT_EQUAL_INT32(50, step.to);
}

void test_a_change_clamped_to_the_same_target_is_inside_the_deadband()
{
    GaugeMotion motion(0, 60, 3);
    motion.Next(60);
    motion.Settled(60);
    TEST_ASSERT_FALSE(motion.Next(90).animate); // clamps to 60, no change
}

// ---- rest and jump ----

void test_next_animation_starts_where_the_needle_settled()
{
    GaugeMotion motion(0, 120, 3);
    motion.Next(40);
    motion.Settled(40);
    motion.Next(90);
    motion.Settled(90);
    TEST_ASSERT_EQUAL_INT32(90, motion.Next(20).from);
}

void test_settled_is_clamped_to_the_scale()
{
    GaugeMotion motion(0, 60, 1);
    motion.Settled(75);
    TEST_ASSERT_EQUAL_INT32(60, motion.Rest());
    motion.Settled(-5);
    TEST_ASSERT_EQUAL_INT32(0, motion.Rest());
}

void test_jump_sets_rest_and_the_deadband_reference()
{
    GaugeMotion motion(0, 120, 3);
    motion.Jump(80);
    TEST_ASSERT_TRUE(motion.HasTarget());
    TEST_ASSERT_EQUAL_INT32(80, motion.Rest());
    TEST_ASSERT_FALSE(motion.Next(82).animate);
    const auto step = motion.Next(90);
    TEST_ASSERT_TRUE(step.animate);
    TEST_ASSERT_EQUAL_INT32(80, step.from);
}

void test_jump_is_clamped_to_the_scale()
{
    GaugeMotion motion(0, 60, 1);
    motion.Jump(200);
    TEST_ASSERT_EQUAL_INT32(60, motion.Rest());
}

void test_reset_makes_the_next_reading_a_first_reading()
{
    GaugeMotion motion(0, 120, 3);
    motion.Next(70);
    motion.Settled(70);
    motion.Reset();
    TEST_ASSERT_FALSE(motion.HasTarget());
    const auto step = motion.Next(70);
    TEST_ASSERT_TRUE(step.animate);
    TEST_ASSERT_EQUAL_INT32(0, step.from);
}

void test_instances_do_not_share_state()
{
    GaugeMotion pressure(0, 60, 1);
    GaugeMotion temperature(0, 120, 3);
    pressure.Next(30);
    pressure.Settled(30);
    TEST_ASSERT_FALSE(temperature.HasTarget());
    TEST_ASSERT_TRUE(temperature.Next(30).animate);
}

void test_set_deadband_changes_later_decisions()
{
    GaugeMotion motion(0, 120, 3);
    motion.Next(50);
    motion.Settled(50);
    TEST_ASSERT_FALSE(motion.Next(52).animate);
    motion.SetDeadband(1);
    TEST_ASSERT_TRUE(motion.Next(52).animate);
}

void test_motion_is_constexpr()
{
    constexpr GaugeMotion motion(0, 60, DeadbandUnits(0, 60, 3));
    static_assert(motion.Deadband() == 1, "3% of 60 floors at one unit");
    static_assert(motion.Clamp(99) == 60, "clamps to the maximum");
    TEST_PASS();
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_deadband_is_a_percent_of_the_range);
    RUN_TEST(test_deadband_is_at_least_one_unit);
    RUN_TEST(test_constructor_floors_the_deadband_at_one);
    RUN_TEST(test_first_reading_always_animates_from_the_minimum);
    RUN_TEST(test_first_reading_at_the_minimum_still_animates);
    RUN_TEST(test_first_reading_above_the_scale_starts_at_the_maximum);
    RUN_TEST(test_first_reading_below_the_scale_is_clamped_to_the_minimum);
    RUN_TEST(test_a_minimum_other_than_zero_is_the_start);
    RUN_TEST(test_change_inside_the_deadband_does_not_animate);
    RUN_TEST(test_change_of_exactly_the_deadband_animates);
    RUN_TEST(test_deadband_applies_in_both_directions);
    RUN_TEST(test_deadband_is_measured_from_the_last_animated_target);
    RUN_TEST(test_skipped_reading_leaves_the_needle_start_alone);
    RUN_TEST(test_a_skipped_step_reports_the_resting_state);
    RUN_TEST(test_a_change_clamped_to_the_same_target_is_inside_the_deadband);
    RUN_TEST(test_next_animation_starts_where_the_needle_settled);
    RUN_TEST(test_settled_is_clamped_to_the_scale);
    RUN_TEST(test_jump_sets_rest_and_the_deadband_reference);
    RUN_TEST(test_jump_is_clamped_to_the_scale);
    RUN_TEST(test_reset_makes_the_next_reading_a_first_reading);
    RUN_TEST(test_instances_do_not_share_state);
    RUN_TEST(test_set_deadband_changes_later_decisions);
    RUN_TEST(test_motion_is_constexpr);
    return UNITY_END();
}
