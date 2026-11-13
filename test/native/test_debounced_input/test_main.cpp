#include <cstdint>

#include <unity.h>

#include "clarity/core/debounced_input.h"

using clarity::core::DebouncedInput;

void setUp()
{
}
void tearDown()
{
}

void test_not_seeded_until_first_sample()
{
    DebouncedInput in(30);
    TEST_ASSERT_FALSE(in.Seeded());
    in.Sample(0, true);
    TEST_ASSERT_TRUE(in.Seeded());
}

void test_first_sample_seeds_state_without_delay_or_change()
{
    DebouncedInput in(30);
    in.Sample(0, true);
    TEST_ASSERT_TRUE(in.State());
    TEST_ASSERT_FALSE(in.Changed());
}

void test_seeded_low_stays_low()
{
    DebouncedInput in(30);
    in.Sample(0, false);
    in.Sample(100, false);
    TEST_ASSERT_FALSE(in.State());
    TEST_ASSERT_FALSE(in.Changed());
}

void test_change_is_accepted_only_after_stable_period()
{
    DebouncedInput in(30);
    in.Sample(0, false);
    in.Sample(10, true);
    TEST_ASSERT_FALSE(in.State());
    in.Sample(39, true);
    TEST_ASSERT_FALSE(in.State());
    TEST_ASSERT_FALSE(in.Changed());
    in.Sample(40, true);
    TEST_ASSERT_TRUE(in.State());
    TEST_ASSERT_TRUE(in.Changed());
}

void test_changed_is_true_for_one_sample_only()
{
    DebouncedInput in(30);
    in.Sample(0, false);
    in.Sample(10, true);
    in.Sample(40, true);
    TEST_ASSERT_TRUE(in.Changed());
    in.Sample(41, true);
    TEST_ASSERT_TRUE(in.State());
    TEST_ASSERT_FALSE(in.Changed());
}

void test_bounce_train_never_changes_state()
{
    DebouncedInput in(30);
    in.Sample(0, false);
    bool raw = true;
    for (uint32_t t = 10; t < 500; t += 10)
    {
        in.Sample(t, raw);
        raw = !raw;
        TEST_ASSERT_FALSE(in.State());
        TEST_ASSERT_FALSE(in.Changed());
    }
}

void test_bounce_then_settle_changes_once_after_last_edge()
{
    DebouncedInput in(30);
    in.Sample(0, false);
    in.Sample(10, true);
    in.Sample(20, false);
    in.Sample(30, true);
    in.Sample(50, true);
    TEST_ASSERT_FALSE(in.State());
    in.Sample(60, true);
    TEST_ASSERT_TRUE(in.State());
    TEST_ASSERT_TRUE(in.Changed());
}

void test_glitch_shorter_than_period_is_ignored()
{
    DebouncedInput in(30);
    in.Sample(0, true);
    in.Sample(10, false);
    in.Sample(20, true);
    in.Sample(100, true);
    TEST_ASSERT_TRUE(in.State());
    TEST_ASSERT_FALSE(in.Changed());
}

void test_change_back_to_original_value()
{
    DebouncedInput in(30);
    in.Sample(0, false);
    in.Sample(10, true);
    in.Sample(40, true);
    TEST_ASSERT_TRUE(in.State());
    in.Sample(50, false);
    in.Sample(80, false);
    TEST_ASSERT_FALSE(in.State());
    TEST_ASSERT_TRUE(in.Changed());
}

void test_zero_stable_period_follows_raw_immediately()
{
    DebouncedInput in(0);
    in.Sample(0, false);
    in.Sample(5, true);
    TEST_ASSERT_TRUE(in.State());
    TEST_ASSERT_TRUE(in.Changed());
}

void test_change_across_millis_wrap()
{
    DebouncedInput in(30);
    in.Sample(UINT32_MAX - 100, false);
    in.Sample(UINT32_MAX - 10, true);
    in.Sample(UINT32_MAX, true);
    TEST_ASSERT_FALSE(in.State());
    in.Sample(18, true); // 29 ms after UINT32_MAX - 10
    TEST_ASSERT_FALSE(in.State());
    in.Sample(19, true); // 30 ms
    TEST_ASSERT_TRUE(in.State());
    TEST_ASSERT_TRUE(in.Changed());
}

void test_bounce_across_millis_wrap_is_ignored()
{
    DebouncedInput in(30);
    in.Sample(UINT32_MAX - 20, false);
    in.Sample(UINT32_MAX - 10, true);
    in.Sample(UINT32_MAX - 5, false);
    in.Sample(5, true);
    in.Sample(25, true);
    TEST_ASSERT_FALSE(in.State());
    in.Sample(35, true);
    TEST_ASSERT_TRUE(in.State());
}

void test_seed_at_wrap_boundary()
{
    DebouncedInput in(30);
    in.Sample(UINT32_MAX, true);
    in.Sample(0, true);
    in.Sample(100, true);
    TEST_ASSERT_TRUE(in.State());
    TEST_ASSERT_FALSE(in.Changed());
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_not_seeded_until_first_sample);
    RUN_TEST(test_first_sample_seeds_state_without_delay_or_change);
    RUN_TEST(test_seeded_low_stays_low);
    RUN_TEST(test_change_is_accepted_only_after_stable_period);
    RUN_TEST(test_changed_is_true_for_one_sample_only);
    RUN_TEST(test_bounce_train_never_changes_state);
    RUN_TEST(test_bounce_then_settle_changes_once_after_last_edge);
    RUN_TEST(test_glitch_shorter_than_period_is_ignored);
    RUN_TEST(test_change_back_to_original_value);
    RUN_TEST(test_zero_stable_period_follows_raw_immediately);
    RUN_TEST(test_change_across_millis_wrap);
    RUN_TEST(test_bounce_across_millis_wrap_is_ignored);
    RUN_TEST(test_seed_at_wrap_boundary);
    return UNITY_END();
}
