#include <cstdint>
#include <cstring>

#include <unity.h>

#include "clarity/core/error_log.h"

using namespace clarity::core;

void setUp()
{
}
void tearDown()
{
}

namespace
{
void fill(ErrorLog &log, ErrorLevel level, uint32_t first_ms, size_t n)
{
    for (size_t i = 0; i < n; ++i)
    {
        log.Report(first_ms + static_cast<uint32_t>(i), level, Fault::DebugTest, "fill", "e%u", static_cast<unsigned>(i));
    }
}
} // namespace

void test_new_log_is_empty()
{
    ErrorLog log;
    TEST_ASSERT_FALSE(log.HasActive());
    TEST_ASSERT_EQUAL_UINT(0, log.Count());
    TEST_ASSERT_EQUAL_UINT32(0, log.Revision());
}

void test_report_stores_all_fields()
{
    ErrorLog log;
    TEST_ASSERT_TRUE(log.Report(1234, ErrorLevel::Error, Fault::NvsWriteFailed, "Storage", "key %s failed (%d)", "abc", 7));
    TEST_ASSERT_TRUE(log.HasActive());
    TEST_ASSERT_EQUAL_UINT(1, log.Count());
    const ErrorInfo &e = log.At(0);
    TEST_ASSERT_EQUAL(ErrorLevel::Error, e.level);
    TEST_ASSERT_EQUAL(Fault::NvsWriteFailed, e.fault);
    TEST_ASSERT_EQUAL_STRING("Storage", e.source);
    TEST_ASSERT_EQUAL_STRING("key abc failed (7)", e.message);
    TEST_ASSERT_EQUAL_UINT32(1234, e.timestamp);
    TEST_ASSERT_FALSE(e.acknowledged);
}

void test_report_without_format_has_empty_message()
{
    ErrorLog log;
    log.Report(0, ErrorLevel::Warning, Fault::DebugTest, "src");
    TEST_ASSERT_EQUAL_STRING("", log.At(0).message);
}

void test_long_message_is_truncated_and_terminated()
{
    ErrorLog log;
    char big[400];
    std::memset(big, 'x', sizeof(big) - 1);
    big[sizeof(big) - 1] = '\0';
    log.Report(0, ErrorLevel::Error, Fault::DebugTest, "src", "%s", big);
    TEST_ASSERT_EQUAL_UINT(ErrorInfo::MESSAGE_SIZE - 1, std::strlen(log.At(0).message));
}

void test_entries_keep_report_order()
{
    ErrorLog log;
    log.Report(1, ErrorLevel::Warning, Fault::DebugTest, "a");
    log.Report(2, ErrorLevel::Critical, Fault::DebugTest, "b");
    log.Report(3, ErrorLevel::Error, Fault::DebugTest, "c");
    TEST_ASSERT_EQUAL_STRING("a", log.At(0).source);
    TEST_ASSERT_EQUAL_STRING("b", log.At(1).source);
    TEST_ASSERT_EQUAL_STRING("c", log.At(2).source);
}

void test_revision_bumps_on_every_report()
{
    ErrorLog log;
    log.Report(0, ErrorLevel::Error, Fault::DebugTest, "a");
    TEST_ASSERT_EQUAL_UINT32(1, log.Revision());
    log.Report(1, ErrorLevel::Error, Fault::DebugTest, "b");
    TEST_ASSERT_EQUAL_UINT32(2, log.Revision());
}

void test_holds_ten_without_loss()
{
    ErrorLog log;
    fill(log, ErrorLevel::Error, 0, 10);
    TEST_ASSERT_EQUAL_UINT(10, log.Count());
    TEST_ASSERT_EQUAL_STRING("e0", log.At(0).message);
    TEST_ASSERT_EQUAL_STRING("e9", log.At(9).message);
}

void test_full_same_level_drops_oldest()
{
    ErrorLog log;
    fill(log, ErrorLevel::Error, 100, 10);
    TEST_ASSERT_TRUE(log.Report(200, ErrorLevel::Error, Fault::DebugTest, "new", "new"));
    TEST_ASSERT_EQUAL_UINT(10, log.Count());
    TEST_ASSERT_EQUAL_STRING("e1", log.At(0).message);
    TEST_ASSERT_EQUAL_STRING("new", log.At(9).message);
}

void test_full_of_errors_rejects_new_warning()
{
    ErrorLog log;
    fill(log, ErrorLevel::Error, 100, 10);
    const uint32_t rev = log.Revision();
    TEST_ASSERT_FALSE(log.Report(200, ErrorLevel::Warning, Fault::DebugTest, "w", "w"));
    TEST_ASSERT_EQUAL_UINT(10, log.Count());
    TEST_ASSERT_EQUAL_STRING("e0", log.At(0).message);
    TEST_ASSERT_EQUAL_UINT32(rev, log.Revision());
}

void test_full_of_warnings_accepts_critical_and_drops_oldest_warning()
{
    ErrorLog log;
    fill(log, ErrorLevel::Warning, 100, 10);
    TEST_ASSERT_TRUE(log.Report(110, ErrorLevel::Critical, Fault::DebugTest, "c", "c"));
    TEST_ASSERT_EQUAL_UINT(10, log.Count());
    TEST_ASSERT_EQUAL_STRING("e1", log.At(0).message);
    TEST_ASSERT_EQUAL_STRING("c", log.At(9).message);
}

void test_trim_drops_lowest_severity_before_older_higher_severity()
{
    ErrorLog log;
    log.Report(0, ErrorLevel::Critical, Fault::DebugTest, "crit0", "crit0"); // oldest of all
    fill(log, ErrorLevel::Error, 10, 8);
    log.Report(50, ErrorLevel::Warning, Fault::DebugTest, "warn", "warn"); // newest, lowest severity
    TEST_ASSERT_EQUAL_UINT(10, log.Count());
    log.Report(60, ErrorLevel::Error, Fault::DebugTest, "err-new", "err-new");
    TEST_ASSERT_EQUAL_UINT(10, log.Count());
    bool has_crit = false;
    bool has_warn = false;
    for (size_t i = 0; i < log.Count(); ++i)
    {
        has_crit |= std::strcmp(log.At(i).source, "crit0") == 0;
        has_warn |= std::strcmp(log.At(i).source, "warn") == 0;
    }
    TEST_ASSERT_TRUE(has_crit);
    TEST_ASSERT_FALSE(has_warn);
}

void test_trim_among_mixed_keeps_highest_then_newest()
{
    ErrorLog log;
    fill(log, ErrorLevel::Critical, 0, 3);
    fill(log, ErrorLevel::Error, 10, 3);
    fill(log, ErrorLevel::Warning, 20, 4);
    log.Report(30, ErrorLevel::Error, Fault::DebugTest, "x", "x");
    // The oldest warning (timestamp 20) goes
    TEST_ASSERT_EQUAL_UINT(10, log.Count());
    uint32_t warnings = 0;
    for (size_t i = 0; i < log.Count(); ++i)
    {
        if (log.At(i).level == ErrorLevel::Warning)
        {
            ++warnings;
            TEST_ASSERT_TRUE(log.At(i).timestamp != 20);
        }
    }
    TEST_ASSERT_EQUAL_UINT32(3, warnings);
}

void test_warning_expires_after_more_than_ten_seconds()
{
    ErrorLog log;
    log.Report(1000, ErrorLevel::Warning, Fault::DebugTest, "w");
    log.Expire(11000);
    TEST_ASSERT_EQUAL_UINT(1, log.Count());
    log.Expire(11001);
    TEST_ASSERT_EQUAL_UINT(0, log.Count());
    TEST_ASSERT_FALSE(log.HasActive());
}

void test_errors_and_critical_never_expire()
{
    ErrorLog log;
    log.Report(0, ErrorLevel::Error, Fault::DebugTest, "e");
    log.Report(0, ErrorLevel::Critical, Fault::DebugTest, "c");
    log.Expire(1000000);
    TEST_ASSERT_EQUAL_UINT(2, log.Count());
}

void test_expire_bumps_revision_only_when_something_expired()
{
    ErrorLog log;
    log.Report(0, ErrorLevel::Warning, Fault::DebugTest, "w");
    const uint32_t rev = log.Revision();
    log.Expire(5000);
    TEST_ASSERT_EQUAL_UINT32(rev, log.Revision());
    log.Expire(20000);
    TEST_ASSERT_EQUAL_UINT32(rev + 1, log.Revision());
    log.Expire(30000);
    TEST_ASSERT_EQUAL_UINT32(rev + 1, log.Revision());
}

void test_expire_keeps_order_of_the_rest()
{
    ErrorLog log;
    log.Report(0, ErrorLevel::Error, Fault::DebugTest, "a");
    log.Report(100, ErrorLevel::Warning, Fault::DebugTest, "w");
    log.Report(20000, ErrorLevel::Error, Fault::DebugTest, "b");
    log.Expire(20000);
    TEST_ASSERT_EQUAL_UINT(2, log.Count());
    TEST_ASSERT_EQUAL_STRING("a", log.At(0).source);
    TEST_ASSERT_EQUAL_STRING("b", log.At(1).source);
}

void test_report_expires_old_warnings_first()
{
    ErrorLog log;
    fill(log, ErrorLevel::Warning, 0, 10);
    // Full of expired warnings: a new warning is accepted because the old ones expire first
    TEST_ASSERT_TRUE(log.Report(20000, ErrorLevel::Warning, Fault::DebugTest, "new", "new"));
    TEST_ASSERT_EQUAL_UINT(1, log.Count());
    TEST_ASSERT_EQUAL_STRING("new", log.At(0).source);
}

void test_expiry_across_millis_wrap()
{
    ErrorLog log;
    log.Report(UINT32_MAX - 1000, ErrorLevel::Warning, Fault::DebugTest, "w");
    log.Expire(8000); // 9001 ms later
    TEST_ASSERT_EQUAL_UINT(1, log.Count());
    log.Expire(9000); // 10001 ms later
    TEST_ASSERT_EQUAL_UINT(0, log.Count());
}

void test_dismiss_removes_warning_and_error_and_shifts()
{
    ErrorLog log;
    log.Report(0, ErrorLevel::Error, Fault::DebugTest, "a");
    log.Report(1, ErrorLevel::Warning, Fault::DebugTest, "b");
    log.Report(2, ErrorLevel::Error, Fault::DebugTest, "c");
    const uint32_t rev = log.Revision();
    log.DismissAt(1);
    TEST_ASSERT_EQUAL_UINT(2, log.Count());
    TEST_ASSERT_EQUAL_STRING("a", log.At(0).source);
    TEST_ASSERT_EQUAL_STRING("c", log.At(1).source);
    TEST_ASSERT_EQUAL_UINT32(rev + 1, log.Revision());
    log.DismissAt(0);
    log.DismissAt(0);
    TEST_ASSERT_FALSE(log.HasActive());
}

void test_dismiss_marks_critical_acknowledged_and_keeps_it()
{
    ErrorLog log;
    log.Report(0, ErrorLevel::Critical, Fault::DebugTest, "c");
    const uint32_t rev = log.Revision();
    log.DismissAt(0);
    TEST_ASSERT_EQUAL_UINT(1, log.Count());
    TEST_ASSERT_TRUE(log.At(0).acknowledged);
    TEST_ASSERT_TRUE(log.HasActive());
    TEST_ASSERT_EQUAL_UINT32(rev + 1, log.Revision());
}

void test_dismiss_out_of_range_is_a_no_op()
{
    ErrorLog log;
    log.Report(0, ErrorLevel::Error, Fault::DebugTest, "a");
    const uint32_t rev = log.Revision();
    log.DismissAt(1);
    log.DismissAt(99);
    TEST_ASSERT_EQUAL_UINT(1, log.Count());
    TEST_ASSERT_EQUAL_UINT32(rev, log.Revision());
}

void test_clear_removes_everything_including_critical()
{
    ErrorLog log;
    log.Report(0, ErrorLevel::Critical, Fault::DebugTest, "c");
    log.Report(0, ErrorLevel::Error, Fault::DebugTest, "e");
    const uint32_t rev = log.Revision();
    log.Clear();
    TEST_ASSERT_FALSE(log.HasActive());
    TEST_ASSERT_EQUAL_UINT(0, log.Count());
    TEST_ASSERT_EQUAL_UINT32(rev + 1, log.Revision());
}

void test_clear_on_empty_log_does_not_bump_revision()
{
    ErrorLog log;
    log.Clear();
    TEST_ASSERT_EQUAL_UINT32(0, log.Revision());
}

void test_at_out_of_range_returns_an_empty_entry()
{
    ErrorLog log;
    const ErrorInfo &e = log.At(3);
    TEST_ASSERT_EQUAL_STRING("", e.message);
    TEST_ASSERT_FALSE(e.acknowledged);
}

void test_log_is_usable_again_after_clear()
{
    ErrorLog log;
    fill(log, ErrorLevel::Error, 0, 10);
    log.Clear();
    fill(log, ErrorLevel::Warning, 0, 3);
    TEST_ASSERT_EQUAL_UINT(3, log.Count());
    TEST_ASSERT_EQUAL_STRING("e0", log.At(0).message);
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_new_log_is_empty);
    RUN_TEST(test_report_stores_all_fields);
    RUN_TEST(test_report_without_format_has_empty_message);
    RUN_TEST(test_long_message_is_truncated_and_terminated);
    RUN_TEST(test_entries_keep_report_order);
    RUN_TEST(test_revision_bumps_on_every_report);
    RUN_TEST(test_holds_ten_without_loss);
    RUN_TEST(test_full_same_level_drops_oldest);
    RUN_TEST(test_full_of_errors_rejects_new_warning);
    RUN_TEST(test_full_of_warnings_accepts_critical_and_drops_oldest_warning);
    RUN_TEST(test_trim_drops_lowest_severity_before_older_higher_severity);
    RUN_TEST(test_trim_among_mixed_keeps_highest_then_newest);
    RUN_TEST(test_warning_expires_after_more_than_ten_seconds);
    RUN_TEST(test_errors_and_critical_never_expire);
    RUN_TEST(test_expire_bumps_revision_only_when_something_expired);
    RUN_TEST(test_expire_keeps_order_of_the_rest);
    RUN_TEST(test_report_expires_old_warnings_first);
    RUN_TEST(test_expiry_across_millis_wrap);
    RUN_TEST(test_dismiss_removes_warning_and_error_and_shifts);
    RUN_TEST(test_dismiss_marks_critical_acknowledged_and_keeps_it);
    RUN_TEST(test_dismiss_out_of_range_is_a_no_op);
    RUN_TEST(test_clear_removes_everything_including_critical);
    RUN_TEST(test_clear_on_empty_log_does_not_bump_revision);
    RUN_TEST(test_at_out_of_range_returns_an_empty_entry);
    RUN_TEST(test_log_is_usable_again_after_clear);
    return UNITY_END();
}
