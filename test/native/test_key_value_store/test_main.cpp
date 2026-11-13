#include <cstdint>
#include <cstring>

#include <unity.h>

#include "clarity/core/in_memory_key_value_store.h"

using namespace clarity::core;

void setUp()
{
}
void tearDown()
{
}

static_assert(FitsKeyLimit("cfg_system"), "short key fits");
static_assert(FitsKeyLimit("cfg_button_sens"), "15 characters fit");
static_assert(!FitsKeyLimit("cfg_button_sensor"), "16 characters do not fit");
static_assert(!FitsKeyLimit(""), "empty does not fit");
static_assert(!FitsKeyLimit("oil_pressure_component.deadband_percent"), "the dotted D-05e key does not fit");

// ---- key limit helper ----

void test_fits_key_limit_boundaries()
{
    TEST_ASSERT_TRUE(FitsKeyLimit("a"));
    TEST_ASSERT_TRUE(FitsKeyLimit("123456789012345"));
    TEST_ASSERT_FALSE(FitsKeyLimit("1234567890123456"));
    TEST_ASSERT_FALSE(FitsKeyLimit(""));
}

// ---- round trips ----

void test_string_round_trip()
{
    InMemoryKeyValueStore s;
    TEST_ASSERT_TRUE(s.PutString("cfg_system", "default_panel", "OemOilPanel"));
    char buf[32];
    TEST_ASSERT_TRUE(s.GetString("cfg_system", "default_panel", buf, sizeof(buf)));
    TEST_ASSERT_EQUAL_STRING("OemOilPanel", buf);
}

void test_int_round_trip_including_negative()
{
    InMemoryKeyValueStore s;
    TEST_ASSERT_TRUE(s.PutInt32("cfg_system", "update_rate", -500));
    int32_t v = 0;
    TEST_ASSERT_TRUE(s.GetInt32("cfg_system", "update_rate", v));
    TEST_ASSERT_EQUAL_INT32(-500, v);
}

void test_bool_round_trip()
{
    InMemoryKeyValueStore s;
    TEST_ASSERT_TRUE(s.PutBool("cfg_system", "show_splash", false));
    bool v = true;
    TEST_ASSERT_TRUE(s.GetBool("cfg_system", "show_splash", v));
    TEST_ASSERT_FALSE(v);
}

void test_float_round_trip()
{
    InMemoryKeyValueStore s;
    TEST_ASSERT_TRUE(s.PutFloat("cfg_oil_pressur", "offset", -2.5f));
    float v = 0.f;
    TEST_ASSERT_TRUE(s.GetFloat("cfg_oil_pressur", "offset", v));
    TEST_ASSERT_EQUAL_FLOAT(-2.5f, v);
}

void test_empty_string_round_trip()
{
    InMemoryKeyValueStore s;
    TEST_ASSERT_TRUE(s.PutString("ns", "k", ""));
    char buf[4] = {'x', 'x', 'x', 'x'};
    TEST_ASSERT_TRUE(s.GetString("ns", "k", buf, sizeof(buf)));
    TEST_ASSERT_EQUAL_STRING("", buf);
}

// ---- missing and untouched ----

void test_missing_key_fails_and_leaves_output_untouched()
{
    InMemoryKeyValueStore s;
    int32_t i = 7;
    bool b = true;
    float f = 1.5f;
    char buf[8] = "keep";
    TEST_ASSERT_FALSE(s.GetInt32("ns", "k", i));
    TEST_ASSERT_FALSE(s.GetBool("ns", "k", b));
    TEST_ASSERT_FALSE(s.GetFloat("ns", "k", f));
    TEST_ASSERT_FALSE(s.GetString("ns", "k", buf, sizeof(buf)));
    TEST_ASSERT_EQUAL_INT32(7, i);
    TEST_ASSERT_TRUE(b);
    TEST_ASSERT_EQUAL_FLOAT(1.5f, f);
    TEST_ASSERT_EQUAL_STRING("keep", buf);
}

void test_namespaces_are_separate()
{
    InMemoryKeyValueStore s;
    s.PutInt32("cfg_oil_pressur", "update_rate", 250);
    s.PutInt32("cfg_oil_tempera", "update_rate", 750);
    int32_t a = 0;
    int32_t b = 0;
    TEST_ASSERT_TRUE(s.GetInt32("cfg_oil_pressur", "update_rate", a));
    TEST_ASSERT_TRUE(s.GetInt32("cfg_oil_tempera", "update_rate", b));
    TEST_ASSERT_EQUAL_INT32(250, a);
    TEST_ASSERT_EQUAL_INT32(750, b);
    TEST_ASSERT_EQUAL_UINT32(2, s.Size());
}

// ---- typing ----

void test_reading_as_another_type_fails()
{
    InMemoryKeyValueStore s;
    s.PutString("cfg_splash_pane", "duration", "1500");
    int32_t v = 9;
    TEST_ASSERT_FALSE(s.GetInt32("cfg_splash_pane", "duration", v));
    TEST_ASSERT_EQUAL_INT32(9, v);
}

void test_put_of_another_type_replaces_the_entry()
{
    InMemoryKeyValueStore s;
    s.PutString("cfg_splash_pane", "duration", "1500");
    TEST_ASSERT_TRUE(s.PutInt32("cfg_splash_pane", "duration", 1500));
    int32_t v = 0;
    char buf[8];
    TEST_ASSERT_TRUE(s.GetInt32("cfg_splash_pane", "duration", v));
    TEST_ASSERT_EQUAL_INT32(1500, v);
    TEST_ASSERT_FALSE(s.GetString("cfg_splash_pane", "duration", buf, sizeof(buf)));
    TEST_ASSERT_EQUAL_UINT32(1, s.Size());
}

void test_overwrite_same_type()
{
    InMemoryKeyValueStore s;
    s.PutInt32("ns", "k", 1);
    s.PutInt32("ns", "k", 2);
    int32_t v = 0;
    s.GetInt32("ns", "k", v);
    TEST_ASSERT_EQUAL_INT32(2, v);
    TEST_ASSERT_EQUAL_UINT32(1, s.Size());
}

// ---- strings and capacity ----

void test_string_that_fills_capacity_exactly_is_accepted()
{
    InMemoryKeyValueStore s;
    s.PutString("ns", "k", "abcd");
    char buf[5];
    TEST_ASSERT_TRUE(s.GetString("ns", "k", buf, sizeof(buf)));
    TEST_ASSERT_EQUAL_STRING("abcd", buf);
}

void test_string_one_byte_too_long_for_capacity_fails_untouched()
{
    InMemoryKeyValueStore s;
    s.PutString("ns", "k", "abcde");
    char buf[5] = "zzzz";
    TEST_ASSERT_FALSE(s.GetString("ns", "k", buf, sizeof(buf)));
    TEST_ASSERT_EQUAL_STRING("zzzz", buf);
}

void test_null_string_value_stores_empty()
{
    InMemoryKeyValueStore s;
    TEST_ASSERT_TRUE(s.PutString("ns", "k", nullptr));
    char buf[4] = "x";
    TEST_ASSERT_TRUE(s.GetString("ns", "k", buf, sizeof(buf)));
    TEST_ASSERT_EQUAL_STRING("", buf);
}

void test_null_output_buffer_fails()
{
    InMemoryKeyValueStore s;
    s.PutString("ns", "k", "a");
    TEST_ASSERT_FALSE(s.GetString("ns", "k", nullptr, 8));
}

// ---- NVS key limit (D-05e) ----

void test_key_over_limit_is_refused_on_write_and_read()
{
    InMemoryKeyValueStore s;
    const char *dotted = "oil_pressure_component.deadband_percent";
    TEST_ASSERT_FALSE(s.PutInt32("cfg_oil_pressur", dotted, 3));
    int32_t v = 0;
    TEST_ASSERT_FALSE(s.GetInt32("cfg_oil_pressur", dotted, v));
    TEST_ASSERT_EQUAL_UINT32(0, s.Size());
    TEST_ASSERT_EQUAL_UINT32(0, s.WriteCount());
}

void test_namespace_over_limit_is_refused()
{
    InMemoryKeyValueStore s;
    TEST_ASSERT_FALSE(s.PutBool("cfg_button_sensor", "k", true));
    TEST_ASSERT_EQUAL_UINT32(0, s.Size());
}

void test_key_of_exactly_fifteen_characters_is_accepted()
{
    InMemoryKeyValueStore s;
    TEST_ASSERT_TRUE(s.PutBool("cfg_button_sens", "123456789012345", true));
}

void test_empty_and_null_names_are_refused()
{
    InMemoryKeyValueStore s;
    TEST_ASSERT_FALSE(s.PutInt32("", "k", 1));
    TEST_ASSERT_FALSE(s.PutInt32("ns", "", 1));
    TEST_ASSERT_FALSE(s.PutInt32(nullptr, "k", 1));
    TEST_ASSERT_FALSE(s.PutInt32("ns", nullptr, 1));
    int32_t v = 0;
    TEST_ASSERT_FALSE(s.GetInt32(nullptr, "k", v));
}

// ---- remove ----

void test_remove_existing_and_missing()
{
    InMemoryKeyValueStore s;
    s.PutInt32("ns", "k", 1);
    TEST_ASSERT_TRUE(s.Remove("ns", "k"));
    int32_t v = 0;
    TEST_ASSERT_FALSE(s.GetInt32("ns", "k", v));
    TEST_ASSERT_FALSE(s.Remove("ns", "k"));
    TEST_ASSERT_FALSE(s.Remove("ns", "this_key_is_much_too_long"));
}

// ---- write failure injection ----

void test_failing_writes_change_nothing()
{
    InMemoryKeyValueStore s;
    s.PutInt32("ns", "k", 1);
    s.SetFailWrites(true);
    TEST_ASSERT_FALSE(s.PutInt32("ns", "k", 2));
    TEST_ASSERT_FALSE(s.PutString("ns", "s", "x"));
    TEST_ASSERT_FALSE(s.Remove("ns", "k"));
    int32_t v = 0;
    TEST_ASSERT_TRUE(s.GetInt32("ns", "k", v));
    TEST_ASSERT_EQUAL_INT32(1, v);
    TEST_ASSERT_EQUAL_UINT32(1, s.WriteCount());
    s.SetFailWrites(false);
    TEST_ASSERT_TRUE(s.PutInt32("ns", "k", 2));
    TEST_ASSERT_EQUAL_UINT32(2, s.WriteCount());
}

void test_usable_through_the_interface()
{
    InMemoryKeyValueStore impl;
    KeyValueStore &s = impl;
    TEST_ASSERT_TRUE(s.PutFloat("ns", "k", 0.5f));
    float v = 0.f;
    TEST_ASSERT_TRUE(s.GetFloat("ns", "k", v));
    TEST_ASSERT_EQUAL_FLOAT(0.5f, v);
}

int main(int, char **)
{
    UNITY_BEGIN();
    RUN_TEST(test_fits_key_limit_boundaries);
    RUN_TEST(test_string_round_trip);
    RUN_TEST(test_int_round_trip_including_negative);
    RUN_TEST(test_bool_round_trip);
    RUN_TEST(test_float_round_trip);
    RUN_TEST(test_empty_string_round_trip);
    RUN_TEST(test_missing_key_fails_and_leaves_output_untouched);
    RUN_TEST(test_namespaces_are_separate);
    RUN_TEST(test_reading_as_another_type_fails);
    RUN_TEST(test_put_of_another_type_replaces_the_entry);
    RUN_TEST(test_overwrite_same_type);
    RUN_TEST(test_string_that_fills_capacity_exactly_is_accepted);
    RUN_TEST(test_string_one_byte_too_long_for_capacity_fails_untouched);
    RUN_TEST(test_null_string_value_stores_empty);
    RUN_TEST(test_null_output_buffer_fails);
    RUN_TEST(test_key_over_limit_is_refused_on_write_and_read);
    RUN_TEST(test_namespace_over_limit_is_refused);
    RUN_TEST(test_key_of_exactly_fifteen_characters_is_accepted);
    RUN_TEST(test_empty_and_null_names_are_refused);
    RUN_TEST(test_remove_existing_and_missing);
    RUN_TEST(test_failing_writes_change_nothing);
    RUN_TEST(test_usable_through_the_interface);
    return UNITY_END();
}
