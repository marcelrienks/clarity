#include <cstring>

#include <unity.h>

#include "clarity/core/types.h"

using namespace clarity::core;

void setUp()
{
}
void tearDown()
{
}

void test_panel_id_names_match_panel_classes()
{
    TEST_ASSERT_EQUAL_STRING("Splash", ToString(PanelId::Splash));
    TEST_ASSERT_EQUAL_STRING("Oil", ToString(PanelId::Oil));
    TEST_ASSERT_EQUAL_STRING("Config", ToString(PanelId::Config));
    TEST_ASSERT_EQUAL_STRING("Error", ToString(PanelId::Error));
    TEST_ASSERT_EQUAL_STRING("Key", ToString(PanelId::Key));
    TEST_ASSERT_EQUAL_STRING("Lock", ToString(PanelId::Lock));
}

void test_theme_id_names_match_stored_values()
{
    TEST_ASSERT_EQUAL_STRING("Day", ToString(ThemeId::Day));
    TEST_ASSERT_EQUAL_STRING("Night", ToString(ThemeId::Night));
    TEST_ASSERT_EQUAL_STRING("Error", ToString(ThemeId::Error));
}

void test_key_state_names()
{
    TEST_ASSERT_EQUAL_STRING("Inactive", ToString(KeyState::Inactive));
    TEST_ASSERT_EQUAL_STRING("Present", ToString(KeyState::Present));
    TEST_ASSERT_EQUAL_STRING("NotPresent", ToString(KeyState::NotPresent));
}

void test_error_level_names()
{
    TEST_ASSERT_EQUAL_STRING("Warning", ToString(ErrorLevel::Warning));
    TEST_ASSERT_EQUAL_STRING("Error", ToString(ErrorLevel::Error));
    TEST_ASSERT_EQUAL_STRING("Critical", ToString(ErrorLevel::Critical));
}

void test_error_level_orders_by_severity()
{
    TEST_ASSERT_TRUE(ErrorLevel::Warning < ErrorLevel::Error);
    TEST_ASSERT_TRUE(ErrorLevel::Error < ErrorLevel::Critical);
}

void test_button_event_names()
{
    TEST_ASSERT_EQUAL_STRING("None", ToString(ButtonEvent::None));
    TEST_ASSERT_EQUAL_STRING("Short", ToString(ButtonEvent::Short));
    TEST_ASSERT_EQUAL_STRING("Long", ToString(ButtonEvent::Long));
}

void test_fault_names()
{
    TEST_ASSERT_EQUAL_STRING("NvsWriteFailed", ToString(Fault::NvsWriteFailed));
    TEST_ASSERT_EQUAL_STRING("SensorImplausible", ToString(Fault::SensorImplausible));
    TEST_ASSERT_EQUAL_STRING("PanelBuildFailed", ToString(Fault::PanelBuildFailed));
    TEST_ASSERT_EQUAL_STRING("DebugTest", ToString(Fault::DebugTest));
}

void test_out_of_range_values_have_a_name()
{
    TEST_ASSERT_EQUAL_STRING("Unknown", ToString(static_cast<PanelId>(250)));
    TEST_ASSERT_EQUAL_STRING("Unknown", ToString(static_cast<ThemeId>(250)));
    TEST_ASSERT_EQUAL_STRING("Unknown", ToString(static_cast<KeyState>(250)));
    TEST_ASSERT_EQUAL_STRING("Unknown", ToString(static_cast<ErrorLevel>(250)));
    TEST_ASSERT_EQUAL_STRING("Unknown", ToString(static_cast<ButtonEvent>(250)));
    TEST_ASSERT_EQUAL_STRING("Unknown", ToString(static_cast<Fault>(60000)));
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_panel_id_names_match_panel_classes);
    RUN_TEST(test_theme_id_names_match_stored_values);
    RUN_TEST(test_key_state_names);
    RUN_TEST(test_error_level_names);
    RUN_TEST(test_error_level_orders_by_severity);
    RUN_TEST(test_button_event_names);
    RUN_TEST(test_fault_names);
    RUN_TEST(test_out_of_range_values_have_a_name);
    return UNITY_END();
}
