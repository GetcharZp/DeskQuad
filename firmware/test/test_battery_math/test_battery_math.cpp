// 校验电量换算与滑动平均

#include <unity.h>

#include <deskquad/core/battery_math.h>

void setUp() {}
void tearDown() {}

static void test_percentage_endpoints()
{
    TEST_ASSERT_EQUAL_INT(0, MapBatteryPercentage(6.0f, 6.4f, 8.4f));
    TEST_ASSERT_EQUAL_INT(0, MapBatteryPercentage(6.4f, 6.4f, 8.4f));
    TEST_ASSERT_EQUAL_INT(100, MapBatteryPercentage(8.4f, 6.4f, 8.4f));
    TEST_ASSERT_EQUAL_INT(100, MapBatteryPercentage(9.9f, 6.4f, 8.4f));
}

static void test_percentage_is_linear()
{
    TEST_ASSERT_EQUAL_INT(50, MapBatteryPercentage(7.4f, 6.4f, 8.4f));
    TEST_ASSERT_EQUAL_INT(25, MapBatteryPercentage(6.9f, 6.4f, 8.4f));
}

static void test_degenerate_range_does_not_divide_by_zero()
{
    TEST_ASSERT_EQUAL_INT(0, MapBatteryPercentage(7.0f, 7.0f, 7.0f));
}

static void test_averager_waits_for_full_window()
{
    BatteryAverager averager(4);
    TEST_ASSERT_FALSE(averager.IsFull());
    averager.Push(100);
    averager.Push(200);
    TEST_ASSERT_FALSE(averager.IsFull());
    averager.Push(300);
    averager.Push(400);
    TEST_ASSERT_TRUE(averager.IsFull());
    TEST_ASSERT_EQUAL_FLOAT(250.0f, averager.Average());
}

static void test_averager_slides()
{
    BatteryAverager averager(3);
    averager.Push(100);
    averager.Push(200);
    averager.Push(300);
    TEST_ASSERT_EQUAL_FLOAT(200.0f, averager.Average());
    averager.Push(600); // 挤掉最早的 100
    TEST_ASSERT_EQUAL_FLOAT(1100.0f / 3.0f, averager.Average());
}

static void test_averager_capacity_is_capped()
{
    BatteryAverager averager(200);
    for (int i = 0; i < BatteryAverager::kMaxSamples; i++)
        averager.Push(10);
    TEST_ASSERT_TRUE(averager.IsFull());
    TEST_ASSERT_EQUAL_FLOAT(10.0f, averager.Average());
}

int main(int, char **)
{
    UNITY_BEGIN();
    RUN_TEST(test_percentage_endpoints);
    RUN_TEST(test_percentage_is_linear);
    RUN_TEST(test_degenerate_range_does_not_divide_by_zero);
    RUN_TEST(test_averager_waits_for_full_window);
    RUN_TEST(test_averager_slides);
    RUN_TEST(test_averager_capacity_is_capped);
    return UNITY_END();
}
