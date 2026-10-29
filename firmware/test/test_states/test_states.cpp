// 校验 action 编号到执行方式的映射
//
// 1-10 走姿态，11-18 走情感动作，0 与越界值不执行任何动作。

#include <unity.h>

#include <deskquad/core/states.h>

void setUp() {}
void tearDown() {}

static void test_pose_range_maps_to_pose()
{
    for (int raw = 1; raw <= 10; raw++)
    {
        const ActionDecision decision = DecideAction(raw);
        TEST_ASSERT_EQUAL(static_cast<int>(ActionDecision::Kind::Pose), static_cast<int>(decision.kind));
        TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(raw), static_cast<uint8_t>(decision.pose));
    }
}

static void test_emotion_range_maps_to_emotion()
{
    for (int raw = 11; raw <= 18; raw++)
    {
        const ActionDecision decision = DecideAction(raw);
        TEST_ASSERT_EQUAL(static_cast<int>(ActionDecision::Kind::Emotion), static_cast<int>(decision.kind));
        TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(raw), static_cast<uint8_t>(decision.emotion));
    }
}

static void test_out_of_range_is_ignored()
{
    const int outOfRange[] = {0, -1, 19, 100, -50};
    for (int raw : outOfRange)
    {
        const ActionDecision decision = DecideAction(raw);
        TEST_ASSERT_EQUAL(static_cast<int>(ActionDecision::Kind::None), static_cast<int>(decision.kind));
    }
}

static void test_runnable_pose_bounds()
{
    TEST_ASSERT_FALSE(IsRunnablePose(ActionId::None));
    TEST_ASSERT_FALSE(IsRunnablePose(ActionId::RandomEmote));
    TEST_ASSERT_TRUE(IsRunnablePose(ActionId::Front));
    TEST_ASSERT_TRUE(IsRunnablePose(ActionId::Sleep));
}

int main(int, char **)
{
    UNITY_BEGIN();
    RUN_TEST(test_pose_range_maps_to_pose);
    RUN_TEST(test_emotion_range_maps_to_emotion);
    RUN_TEST(test_out_of_range_is_ignored);
    RUN_TEST(test_runnable_pose_bounds);
    return UNITY_END();
}
