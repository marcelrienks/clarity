#include <initializer_list>

#include <unity.h>

#include "clarity/core/display_policy.h"

using namespace clarity::core;

void setUp()
{
}
void tearDown()
{
}

namespace
{
constexpr VehicleInputs inputs(bool key_present, bool key_not_present, bool lock, bool lights)
{
    return VehicleInputs{key_present, key_not_present, lock, lights};
}

void expect_request(OverridePanel panel, KeyState key, const DisplayRequest &actual)
{
    TEST_ASSERT_EQUAL(panel, actual.panel);
    TEST_ASSERT_EQUAL(key, actual.key);
}
} // namespace

// ---- ResolveDisplay: scenario table (docs/restructure.md A-02) ----

void test_nothing_active_has_no_override()
{
    expect_request(OverridePanel::None, KeyState::Inactive, ResolveDisplay(inputs(0, 0, 0, 0), false));
}

void test_lights_alone_are_not_an_override()
{
    expect_request(OverridePanel::None, KeyState::Inactive, ResolveDisplay(inputs(0, 0, 0, 1), false));
}

void test_lock_shows_lock_panel()
{
    expect_request(OverridePanel::Lock, KeyState::Inactive, ResolveDisplay(inputs(0, 0, 1, 0), false));
    expect_request(OverridePanel::Lock, KeyState::Inactive, ResolveDisplay(inputs(0, 0, 1, 1), false));
}

void test_key_present_shows_key_panel_present()
{
    expect_request(OverridePanel::Key, KeyState::Present, ResolveDisplay(inputs(1, 0, 0, 0), false));
}

void test_key_not_present_shows_key_panel_not_present()
{
    expect_request(OverridePanel::Key, KeyState::NotPresent, ResolveDisplay(inputs(0, 1, 0, 0), false));
}

void test_key_beats_lock()
{
    expect_request(OverridePanel::Key, KeyState::NotPresent, ResolveDisplay(inputs(0, 1, 1, 1), false)); // step 5
    expect_request(OverridePanel::Key, KeyState::Present, ResolveDisplay(inputs(1, 0, 1, 1), false));    // step 6
}

void test_both_key_inputs_active_present_wins()
{
    expect_request(OverridePanel::Key, KeyState::Present, ResolveDisplay(inputs(1, 1, 0, 0), false));
    expect_request(OverridePanel::Key, KeyState::Present, ResolveDisplay(inputs(1, 1, 1, 1), false));
}

void test_errors_beat_everything()
{
    expect_request(OverridePanel::Error, KeyState::Inactive, ResolveDisplay(inputs(0, 0, 0, 0), true));
    expect_request(OverridePanel::Error, KeyState::Inactive, ResolveDisplay(inputs(0, 1, 1, 1), true)); // step 9
    expect_request(OverridePanel::Error, KeyState::Inactive, ResolveDisplay(inputs(1, 1, 1, 1), true));
}

void test_error_while_lock_active()
{
    expect_request(OverridePanel::Error, KeyState::Inactive, ResolveDisplay(inputs(0, 0, 1, 0), true));
}

void test_errors_cleared_returns_to_key_override() // step 11
{
    expect_request(OverridePanel::Key, KeyState::NotPresent, ResolveDisplay(inputs(0, 1, 1, 1), false));
}

void test_all_off_except_lights_has_no_override() // step 12
{
    expect_request(OverridePanel::None, KeyState::Inactive, ResolveDisplay(inputs(0, 0, 0, 1), false));
}

void test_boot_with_lock_active_is_a_lock_override()
{
    expect_request(OverridePanel::Lock, KeyState::Inactive, ResolveDisplay(inputs(0, 0, 1, 0), false));
}

void test_key_state_is_inactive_unless_the_key_panel_is_requested()
{
    for (int bits = 0; bits < 32; ++bits)
    {
        const VehicleInputs in = inputs(bits & 1, bits & 2, bits & 4, bits & 8);
        const bool errors = (bits & 16) != 0;
        const DisplayRequest r = ResolveDisplay(in, errors);
        if (r.panel != OverridePanel::Key)
        {
            TEST_ASSERT_EQUAL(KeyState::Inactive, r.key);
        }
        else
        {
            TEST_ASSERT_TRUE(r.key != KeyState::Inactive);
        }
    }
}

void test_exhaustive_priority_error_key_lock_none()
{
    for (int bits = 0; bits < 32; ++bits)
    {
        const bool kp = bits & 1;
        const bool knp = bits & 2;
        const bool lock = bits & 4;
        const bool lights = bits & 8;
        const bool errors = bits & 16;
        const DisplayRequest r = ResolveDisplay(inputs(kp, knp, lock, lights), errors);
        OverridePanel want = OverridePanel::None;
        if (errors)
        {
            want = OverridePanel::Error;
        }
        else if (kp || knp)
        {
            want = OverridePanel::Key;
        }
        else if (lock)
        {
            want = OverridePanel::Lock;
        }
        TEST_ASSERT_EQUAL_MESSAGE(want, r.panel, "priority order");
    }
}

void test_lights_never_change_the_requested_panel()
{
    for (int bits = 0; bits < 32; ++bits)
    {
        const VehicleInputs off = inputs(bits & 1, bits & 2, bits & 4, false);
        const VehicleInputs on = inputs(bits & 1, bits & 2, bits & 4, true);
        const bool errors = (bits & 16) != 0;
        TEST_ASSERT_EQUAL(ResolveDisplay(off, errors).panel, ResolveDisplay(on, errors).panel);
        TEST_ASSERT_EQUAL(ResolveDisplay(off, errors).key, ResolveDisplay(on, errors).key);
    }
}

void test_resolve_display_is_constexpr()
{
    static_assert(ResolveDisplay(VehicleInputs{false, false, true, false}, false).panel == OverridePanel::Lock, "lock");
    TEST_PASS();
}

// ---- EffectiveTheme (Q-05: the theme follows the dash-light signal only) ----

void test_lights_on_gives_night()
{
    const VehicleInputs lights = inputs(0, 0, 0, 1);
    TEST_ASSERT_EQUAL(ThemeId::Night, EffectiveTheme(ResolveDisplay(lights, false), lights));
}

void test_lights_off_gives_day() // step 13
{
    const VehicleInputs dark = inputs(0, 0, 0, 0);
    TEST_ASSERT_EQUAL(ThemeId::Day, EffectiveTheme(ResolveDisplay(dark, false), dark));
}

void test_error_panel_forces_error_theme_with_lights_on_or_off()
{
    for (int lights = 0; lights < 2; ++lights)
    {
        const VehicleInputs in = inputs(0, 1, 1, lights != 0); // step 9
        TEST_ASSERT_EQUAL(ThemeId::Error, EffectiveTheme(ResolveDisplay(in, true), in));
    }
}

void test_theme_follows_lights_during_key_and_lock_overrides()
{
    const VehicleInputs lock_lights = inputs(0, 0, 1, 1);
    TEST_ASSERT_EQUAL(ThemeId::Night, EffectiveTheme(ResolveDisplay(lock_lights, false), lock_lights));
    const VehicleInputs key_dark = inputs(1, 0, 0, 0);
    TEST_ASSERT_EQUAL(ThemeId::Day, EffectiveTheme(ResolveDisplay(key_dark, false), key_dark));
}

void test_boot_with_override_and_lights_uses_night()
{
    const VehicleInputs in = inputs(0, 0, 1, 1);
    TEST_ASSERT_EQUAL(ThemeId::Night, EffectiveTheme(ResolveDisplay(in, false), in));
}

void test_boot_with_lights_off_uses_day()
{
    const VehicleInputs in = inputs(0, 0, 0, 0);
    TEST_ASSERT_EQUAL(ThemeId::Day, EffectiveTheme(ResolveDisplay(in, false), in));
}

void test_errors_cleared_theme_returns_to_lights() // step 11
{
    const VehicleInputs in = inputs(0, 1, 1, 1);
    TEST_ASSERT_EQUAL(ThemeId::Error, EffectiveTheme(ResolveDisplay(in, true), in));
    TEST_ASSERT_EQUAL(ThemeId::Night, EffectiveTheme(ResolveDisplay(in, false), in));
}

void test_theme_is_a_pure_function_of_errors_and_lights()
{
    for (int bits = 0; bits < 32; ++bits)
    {
        const VehicleInputs in = inputs(bits & 1, bits & 2, bits & 4, bits & 8);
        const bool errors = (bits & 16) != 0;
        const ThemeId want = errors ? ThemeId::Error : (in.lights ? ThemeId::Night : ThemeId::Day);
        TEST_ASSERT_EQUAL(want, EffectiveTheme(ResolveDisplay(in, errors), in));
    }
}

void test_no_latch_lights_off_after_on_is_day_again()
{
    const VehicleInputs on = inputs(0, 0, 0, 1);
    const VehicleInputs off = inputs(0, 0, 0, 0);
    TEST_ASSERT_EQUAL(ThemeId::Night, EffectiveTheme(ResolveDisplay(on, false), on));
    TEST_ASSERT_EQUAL(ThemeId::Day, EffectiveTheme(ResolveDisplay(off, false), off));
    TEST_ASSERT_EQUAL(ThemeId::Night, EffectiveTheme(ResolveDisplay(on, false), on));
}

void test_scenario_sequence_themes()
{
    struct Step
    {
        VehicleInputs in;
        bool errors;
        OverridePanel panel;
        ThemeId theme;
    };
    const Step steps[] = {
        {inputs(0, 0, 0, 1), false, OverridePanel::None, ThemeId::Night}, // 3 lights on
        {inputs(0, 0, 1, 1), false, OverridePanel::Lock, ThemeId::Night}, // 4 lock
        {inputs(0, 1, 1, 1), false, OverridePanel::Key, ThemeId::Night},  // 5 key not present
        {inputs(1, 0, 1, 1), false, OverridePanel::Key, ThemeId::Night},  // 6 key present
        {inputs(0, 1, 1, 1), false, OverridePanel::Key, ThemeId::Night},  // 8 key present off
        {inputs(0, 1, 1, 1), true, OverridePanel::Error, ThemeId::Error}, // 9 debug error
        {inputs(0, 1, 1, 1), false, OverridePanel::Key, ThemeId::Night},  // 11 errors cleared
        {inputs(0, 0, 0, 1), false, OverridePanel::None, ThemeId::Night}, // 12 all off except lights
        {inputs(0, 0, 0, 0), false, OverridePanel::None, ThemeId::Day},   // 13 lights off
    };
    for (const Step &s : steps)
    {
        const DisplayRequest r = ResolveDisplay(s.in, s.errors);
        TEST_ASSERT_EQUAL(s.panel, r.panel);
        TEST_ASSERT_EQUAL(s.theme, EffectiveTheme(r, s.in));
    }
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_nothing_active_has_no_override);
    RUN_TEST(test_lights_alone_are_not_an_override);
    RUN_TEST(test_lock_shows_lock_panel);
    RUN_TEST(test_key_present_shows_key_panel_present);
    RUN_TEST(test_key_not_present_shows_key_panel_not_present);
    RUN_TEST(test_key_beats_lock);
    RUN_TEST(test_both_key_inputs_active_present_wins);
    RUN_TEST(test_errors_beat_everything);
    RUN_TEST(test_error_while_lock_active);
    RUN_TEST(test_errors_cleared_returns_to_key_override);
    RUN_TEST(test_all_off_except_lights_has_no_override);
    RUN_TEST(test_boot_with_lock_active_is_a_lock_override);
    RUN_TEST(test_key_state_is_inactive_unless_the_key_panel_is_requested);
    RUN_TEST(test_exhaustive_priority_error_key_lock_none);
    RUN_TEST(test_lights_never_change_the_requested_panel);
    RUN_TEST(test_resolve_display_is_constexpr);
    RUN_TEST(test_lights_on_gives_night);
    RUN_TEST(test_lights_off_gives_day);
    RUN_TEST(test_error_panel_forces_error_theme_with_lights_on_or_off);
    RUN_TEST(test_theme_follows_lights_during_key_and_lock_overrides);
    RUN_TEST(test_boot_with_override_and_lights_uses_night);
    RUN_TEST(test_boot_with_lights_off_uses_day);
    RUN_TEST(test_errors_cleared_theme_returns_to_lights);
    RUN_TEST(test_theme_is_a_pure_function_of_errors_and_lights);
    RUN_TEST(test_no_latch_lights_off_after_on_is_day_again);
    RUN_TEST(test_scenario_sequence_themes);
    return UNITY_END();
}
