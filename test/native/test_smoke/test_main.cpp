#include <unity.h>

#include "clarity/core/version.h"

void setUp()
{
}
void tearDown()
{
}

void test_core_api_version_is_positive()
{
    TEST_ASSERT_GREATER_THAN_INT(0, clarity::core::API_VERSION);
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_core_api_version_is_positive);
    return UNITY_END();
}
