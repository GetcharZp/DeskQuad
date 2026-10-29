// 校验姿态表的数据完整性

#include <unity.h>

#include <deskquad/core/pose_table.h>
#include <deskquad/core/states.h>

namespace
{

    bool IsServoAngleValid(int16_t angle)
    {
        return angle == kHoldAngle || (angle >= 0 && angle <= 180);
    }

    // 遍历所有编号，方便逐个检查可执行姿态
    template <typename Fn> void ForEachRunnablePose(Fn fn)
    {
        for (uint8_t raw = 0; raw <= static_cast<uint8_t>(ActionId::Sleep); raw++)
        {
            const ActionId id = static_cast<ActionId>(raw);
            if (IsRunnablePose(id))
                fn(id);
        }
    }

} // namespace

void setUp() {}
void tearDown() {}

static void test_runnable_poses_have_programs()
{
    ForEachRunnablePose([](ActionId id) { TEST_ASSERT_NOT_NULL(FindPoseProgram(id)); });
}

static void test_non_pose_ids_have_no_program()
{
    TEST_ASSERT_NULL(FindPoseProgram(ActionId::None));
    TEST_ASSERT_NULL(FindPoseProgram(ActionId::RandomEmote));
}

static void test_program_structure()
{
    ForEachRunnablePose([](ActionId id) {
        const PoseProgram *program = FindPoseProgram(id);
        TEST_ASSERT_NOT_NULL(program);
        TEST_ASSERT_TRUE(program->stepCount > 0);
        TEST_ASSERT_TRUE(program->loopSteps >= 1);
        TEST_ASSERT_TRUE(program->loopSteps <= program->stepCount);
        TEST_ASSERT_TRUE(program->loopCount >= 1);
    });
}

static void test_step_angles_and_holds()
{
    ForEachRunnablePose([](ActionId id) {
        const PoseProgram *program = FindPoseProgram(id);
        TEST_ASSERT_NOT_NULL(program);

        for (uint8_t index = 0; index < program->stepCount; index++)
        {
            const PoseStep &step = program->steps[index];
            for (uint8_t channel = 0; channel < 4; channel++)
                TEST_ASSERT_TRUE(IsServoAngleValid(step.servo[channel]));

            // 末步的 0 表示播放结束；中间步骤必须停留一段时间
            if (index + 1 < program->stepCount)
                TEST_ASSERT_TRUE(step.holdMs > 0);
        }
    });
}

int main(int, char **)
{
    UNITY_BEGIN();
    RUN_TEST(test_runnable_poses_have_programs);
    RUN_TEST(test_non_pose_ids_have_no_program);
    RUN_TEST(test_program_structure);
    RUN_TEST(test_step_angles_and_holds);
    return UNITY_END();
}
