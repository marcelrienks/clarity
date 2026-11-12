#include <cstdint>

#include <unity.h>

#include "clarity/core/button_fsm.h"
#include "clarity/core/debounced_input.h"

using clarity::core::ButtonEvent;
using clarity::core::ButtonFsm;
using clarity::core::DebouncedInput;

namespace
{
constexpr uint32_t MIN_SHORT = 50;
constexpr uint32_t LONG_PRESS = 1500;
} // namespace

void setUp()
{
}
void tearDown()
{
}

void test_idle_emits_nothing()
{
    ButtonFsm fsm(MIN_SHORT, LONG_PRESS);
    for (uint32_t t = 0; t < 5000; t += 100)
    {
        TEST_ASSERT_EQUAL(ButtonEvent::None, fsm.Update(t, false));
    }
}

void test_release_below_minimum_is_ignored()
{
    ButtonFsm fsm(MIN_SHORT, LONG_PRESS);
    TEST_ASSERT_EQUAL(ButtonEvent::None, fsm.Update(1000, true));
    TEST_ASSERT_EQUAL(ButtonEvent::None, fsm.Update(1049, false));
    TEST_ASSERT_EQUAL(ButtonEvent::None, fsm.Update(1100, false));
}

void test_release_at_minimum_is_short()
{
    ButtonFsm fsm(MIN_SHORT, LONG_PRESS);
    fsm.Update(1000, true);
    TEST_ASSERT_EQUAL(ButtonEvent::Short, fsm.Update(1050, false));
}

void test_short_press_emits_on_release_not_on_press()
{
    ButtonFsm fsm(MIN_SHORT, LONG_PRESS);
    TEST_ASSERT_EQUAL(ButtonEvent::None, fsm.Update(0, true));
    TEST_ASSERT_EQUAL(ButtonEvent::None, fsm.Update(200, true));
    TEST_ASSERT_EQUAL(ButtonEvent::Short, fsm.Update(400, false));
}

void test_release_just_below_threshold_is_short()
{
    ButtonFsm fsm(MIN_SHORT, LONG_PRESS);
    fsm.Update(0, true);
    TEST_ASSERT_EQUAL(ButtonEvent::None, fsm.Update(1499, true));
    TEST_ASSERT_EQUAL(ButtonEvent::Short, fsm.Update(1499, false));
}

void test_long_fires_at_exactly_the_threshold_while_held()
{
    ButtonFsm fsm(MIN_SHORT, LONG_PRESS);
    fsm.Update(0, true);
    TEST_ASSERT_EQUAL(ButtonEvent::None, fsm.Update(1499, true));
    TEST_ASSERT_EQUAL(ButtonEvent::Long, fsm.Update(1500, true));
}

void test_long_fires_once_while_held_past_threshold()
{
    ButtonFsm fsm(MIN_SHORT, LONG_PRESS);
    fsm.Update(0, true);
    int longs = 0;
    for (uint32_t t = 1000; t <= 10000; t += 10)
    {
        ButtonEvent ev = fsm.Update(t, true);
        TEST_ASSERT_TRUE(ev == ButtonEvent::None || ev == ButtonEvent::Long);
        longs += ev == ButtonEvent::Long ? 1 : 0;
    }
    TEST_ASSERT_EQUAL_INT(1, longs);
}

void test_release_after_long_emits_nothing()
{
    ButtonFsm fsm(MIN_SHORT, LONG_PRESS);
    fsm.Update(0, true);
    TEST_ASSERT_EQUAL(ButtonEvent::Long, fsm.Update(1500, true));
    TEST_ASSERT_EQUAL(ButtonEvent::None, fsm.Update(2000, false));
    TEST_ASSERT_EQUAL(ButtonEvent::None, fsm.Update(2100, false));
}

void test_release_sample_past_threshold_without_prior_sample_is_long()
{
    ButtonFsm fsm(MIN_SHORT, LONG_PRESS);
    fsm.Update(0, true);
    TEST_ASSERT_EQUAL(ButtonEvent::Long, fsm.Update(2000, false));
    TEST_ASSERT_EQUAL(ButtonEvent::None, fsm.Update(2010, false));
}

void test_new_press_after_long_works()
{
    ButtonFsm fsm(MIN_SHORT, LONG_PRESS);
    fsm.Update(0, true);
    fsm.Update(1500, true);
    fsm.Update(2000, false);
    fsm.Update(3000, true);
    TEST_ASSERT_EQUAL(ButtonEvent::Short, fsm.Update(3100, false));
}

void test_back_to_back_short_presses()
{
    ButtonFsm fsm(MIN_SHORT, LONG_PRESS);
    fsm.Update(0, true);
    TEST_ASSERT_EQUAL(ButtonEvent::Short, fsm.Update(100, false));
    fsm.Update(150, true);
    TEST_ASSERT_EQUAL(ButtonEvent::Short, fsm.Update(300, false));
}

void test_raising_threshold_mid_press_delays_long()
{
    ButtonFsm fsm(MIN_SHORT, LONG_PRESS);
    fsm.Update(0, true);
    fsm.Update(1000, true);
    fsm.SetThresholds(MIN_SHORT, 3000);
    TEST_ASSERT_EQUAL(ButtonEvent::None, fsm.Update(1500, true));
    TEST_ASSERT_EQUAL(ButtonEvent::None, fsm.Update(2999, true));
    TEST_ASSERT_EQUAL(ButtonEvent::Long, fsm.Update(3000, true));
}

void test_lowering_threshold_mid_press_fires_long_at_next_sample()
{
    ButtonFsm fsm(MIN_SHORT, 3000);
    fsm.Update(0, true);
    fsm.Update(1000, true);
    fsm.SetThresholds(MIN_SHORT, 1000);
    TEST_ASSERT_EQUAL(ButtonEvent::Long, fsm.Update(1010, true));
}

void test_threshold_change_does_not_affect_idle_button()
{
    ButtonFsm fsm(MIN_SHORT, LONG_PRESS);
    fsm.SetThresholds(MIN_SHORT, 1000);
    fsm.Update(0, true);
    TEST_ASSERT_EQUAL(ButtonEvent::Long, fsm.Update(1000, true));
}

void test_press_and_long_across_millis_wrap()
{
    ButtonFsm fsm(MIN_SHORT, LONG_PRESS);
    fsm.Update(UINT32_MAX - 500, true);
    TEST_ASSERT_EQUAL(ButtonEvent::None, fsm.Update(998, true)); // 1499 ms held
    TEST_ASSERT_EQUAL(ButtonEvent::Long, fsm.Update(999, true)); // 1500 ms held
}

void test_short_across_millis_wrap()
{
    ButtonFsm fsm(MIN_SHORT, LONG_PRESS);
    fsm.Update(UINT32_MAX - 20, true);
    TEST_ASSERT_EQUAL(ButtonEvent::Short, fsm.Update(100, false));
}

void test_bounce_through_debouncer_gives_one_short()
{
    DebouncedInput in(MIN_SHORT);
    ButtonFsm fsm(MIN_SHORT, LONG_PRESS);
    int shorts = 0;
    int others = 0;
    auto step = [&](uint32_t t, bool raw)
    {
        in.Sample(t, raw);
        ButtonEvent ev = fsm.Update(t, in.State());
        shorts += ev == ButtonEvent::Short ? 1 : 0;
        others += (ev == ButtonEvent::Long) ? 1 : 0;
    };
    step(0, false);
    // Contact bounce on press, then held 300 ms, then bounce on release
    step(100, true);
    step(105, false);
    step(110, true);
    step(115, false);
    step(120, true);
    for (uint32_t t = 130; t < 420; t += 10)
    {
        step(t, true);
    }
    step(420, false);
    step(425, true);
    step(430, false);
    for (uint32_t t = 440; t < 700; t += 10)
    {
        step(t, false);
    }
    TEST_ASSERT_EQUAL_INT(1, shorts);
    TEST_ASSERT_EQUAL_INT(0, others);
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_idle_emits_nothing);
    RUN_TEST(test_release_below_minimum_is_ignored);
    RUN_TEST(test_release_at_minimum_is_short);
    RUN_TEST(test_short_press_emits_on_release_not_on_press);
    RUN_TEST(test_release_just_below_threshold_is_short);
    RUN_TEST(test_long_fires_at_exactly_the_threshold_while_held);
    RUN_TEST(test_long_fires_once_while_held_past_threshold);
    RUN_TEST(test_release_after_long_emits_nothing);
    RUN_TEST(test_release_sample_past_threshold_without_prior_sample_is_long);
    RUN_TEST(test_new_press_after_long_works);
    RUN_TEST(test_back_to_back_short_presses);
    RUN_TEST(test_raising_threshold_mid_press_delays_long);
    RUN_TEST(test_lowering_threshold_mid_press_fires_long_at_next_sample);
    RUN_TEST(test_threshold_change_does_not_affect_idle_button);
    RUN_TEST(test_press_and_long_across_millis_wrap);
    RUN_TEST(test_short_across_millis_wrap);
    RUN_TEST(test_bounce_through_debouncer_gives_one_short);
    return UNITY_END();
}
