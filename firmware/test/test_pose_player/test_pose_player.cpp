// 校验姿态序列的数据与播放时序

#include <unity.h>

#include <vector>

#include <deskquad/core/pose_player.h>
#include <deskquad/core/pose_table.h>

// native 平台的时间源，定义在 src/platform/clock.cpp
void SetNowMs(unsigned long ms);
void AdvanceNowMs(unsigned long ms);

namespace
{

    // ServoWrite 记录一次 (通道, 角度) 写入
    struct ServoWrite
    {
        uint8_t channel;
        int16_t angle;
    };

    std::vector<ServoWrite> gLog;

    class RecordingServoWriter : public IServoWriter
    {
    public:
        void Write(uint8_t channel, int16_t angle) override { gLog.push_back(ServoWrite{channel, angle}); }
    };

    void advance(unsigned long ms)
    {
        AdvanceNowMs(ms);
    }

    void resetLog()
    {
        gLog.clear();
        SetNowMs(0);
    }

    bool hasWrite(uint8_t channel, int16_t angle)
    {
        for (const ServoWrite &w : gLog)
        {
            if (w.channel == channel && w.angle == angle)
                return true;
        }
        return false;
    }

    int countWrites()
    {
        return static_cast<int>(gLog.size());
    }

} // namespace

void setUp() {}
void tearDown() {}

static void test_only_real_poses_have_programs()
{
    TEST_ASSERT_NOT_NULL(FindPoseProgram(ActionId::Front));
    TEST_ASSERT_NOT_NULL(FindPoseProgram(ActionId::Back));
    TEST_ASSERT_NOT_NULL(FindPoseProgram(ActionId::Left));
    TEST_ASSERT_NOT_NULL(FindPoseProgram(ActionId::Right));
    TEST_ASSERT_NOT_NULL(FindPoseProgram(ActionId::HandLeft));
    TEST_ASSERT_NOT_NULL(FindPoseProgram(ActionId::HandRight));
    TEST_ASSERT_NOT_NULL(FindPoseProgram(ActionId::Lie));
    TEST_ASSERT_NOT_NULL(FindPoseProgram(ActionId::Sit));
    TEST_ASSERT_NOT_NULL(FindPoseProgram(ActionId::Sleep));

    TEST_ASSERT_NULL(FindPoseProgram(ActionId::None));
    TEST_ASSERT_NULL(FindPoseProgram(ActionId::RandomEmote));
}

static void test_loop_metadata_matches_original()
{
    // 左转与右转在原实现里循环三次
    TEST_ASSERT_EQUAL_UINT8(3, FindPoseProgram(ActionId::Left)->loopCount);
    TEST_ASSERT_EQUAL_UINT8(3, FindPoseProgram(ActionId::Right)->loopCount);
    TEST_ASSERT_EQUAL_UINT8(10, FindPoseProgram(ActionId::Left)->stepCount);

    // 抬左手/抬右手摆动三次后归位：前两步循环，最后一步只走一次
    const PoseProgram *handLeft = FindPoseProgram(ActionId::HandLeft);
    TEST_ASSERT_EQUAL_UINT8(3, handLeft->stepCount);
    TEST_ASSERT_EQUAL_UINT8(2, handLeft->loopSteps);
    TEST_ASSERT_EQUAL_UINT8(3, handLeft->loopCount);

    const PoseProgram *front = FindPoseProgram(ActionId::Front);
    TEST_ASSERT_EQUAL_UINT8(8, front->stepCount);
    TEST_ASSERT_EQUAL_UINT8(1, front->loopCount);
}

static void test_front_angles_match_original()
{
    const PoseProgram *front = FindPoseProgram(ActionId::Front);
    // 原 front(): (s2=140,s3=40) → (s1=40,s4=140) → (s2=90,s3=90)
    //             → (s1=90,s4=90) → (s1=140,s4=40) → (s2=40,s3=140)
    //             → (s1=90,s4=90) → (s2=90,s3=90)
    TEST_ASSERT_EQUAL_INT16(140, front->steps[0].servo[1]);
    TEST_ASSERT_EQUAL_INT16(40, front->steps[0].servo[2]);
    TEST_ASSERT_EQUAL_INT16(kHoldAngle, front->steps[0].servo[0]);
    TEST_ASSERT_EQUAL_INT16(40, front->steps[1].servo[0]);
    TEST_ASSERT_EQUAL_INT16(140, front->steps[1].servo[3]);
    TEST_ASSERT_EQUAL_INT16(140, front->steps[4].servo[0]);
    TEST_ASSERT_EQUAL_INT16(40, front->steps[4].servo[3]);
    TEST_ASSERT_EQUAL_INT16(40, front->steps[5].servo[1]);
    TEST_ASSERT_EQUAL_INT16(140, front->steps[5].servo[2]);
    // 最后一步后面没有延时
    TEST_ASSERT_EQUAL_UINT16(0, front->steps[7].holdMs);
}

static void test_sit_and_sleep_match_original()
{
    const PoseProgram *sit = FindPoseProgram(ActionId::Sit);
    // 原 sitdown(): (s2=140,s4=40) delay(3000) → (s2=90,s4=90)
    TEST_ASSERT_EQUAL_INT16(140, sit->steps[0].servo[1]);
    TEST_ASSERT_EQUAL_INT16(40, sit->steps[0].servo[3]);
    TEST_ASSERT_EQUAL_UINT16(3000, sit->steps[0].holdMs);
    TEST_ASSERT_EQUAL_INT16(90, sit->steps[1].servo[1]);
    TEST_ASSERT_EQUAL_INT16(90, sit->steps[1].servo[3]);

    const PoseProgram *sleep = FindPoseProgram(ActionId::Sleep);
    // 原 dosleep(): (s1=0,s2=180,s3=180,s4=0)，四个通道都写且没有延时
    TEST_ASSERT_EQUAL_INT16(0, sleep->steps[0].servo[0]);
    TEST_ASSERT_EQUAL_INT16(180, sleep->steps[0].servo[1]);
    TEST_ASSERT_EQUAL_INT16(180, sleep->steps[0].servo[2]);
    TEST_ASSERT_EQUAL_INT16(0, sleep->steps[0].servo[3]);
    TEST_ASSERT_EQUAL_UINT16(0, sleep->steps[0].holdMs);
}

static void test_player_writes_first_step_immediately()
{
    resetLog();
    RecordingServoWriter writer;
    PosePlayer player(writer);

    player.Play(*FindPoseProgram(ActionId::Front));

    TEST_ASSERT_TRUE(player.IsPlaying());
    TEST_ASSERT_EQUAL_INT(2, countWrites());
    TEST_ASSERT_TRUE(hasWrite(1, 140));
    TEST_ASSERT_TRUE(hasWrite(2, 40));
}

static void test_player_ignores_hold_channels()
{
    resetLog();
    RecordingServoWriter writer;
    PosePlayer player(writer);
    player.Play(*FindPoseProgram(ActionId::Front));

    const int before = countWrites();
    advance(100);
    player.Update();

    // 第二步只写 s1 与 s4，不再重复写 s2/s3
    TEST_ASSERT_EQUAL_INT(before + 2, countWrites());
    TEST_ASSERT_TRUE(hasWrite(0, 40));
    TEST_ASSERT_TRUE(hasWrite(3, 140));
}

static void test_player_finishes_after_last_step()
{
    resetLog();
    RecordingServoWriter writer;
    PosePlayer player(writer);
    player.Play(*FindPoseProgram(ActionId::Front));

    for (int i = 0; i < 8 && player.IsPlaying(); i++)
    {
        advance(100);
        player.Update();
    }
    TEST_ASSERT_FALSE(player.IsPlaying());
    // 八步，每步两个通道
    TEST_ASSERT_EQUAL_INT(16, countWrites());
}

static void test_player_repeats_left_three_times()
{
    resetLog();
    RecordingServoWriter writer;
    PosePlayer player(writer);
    player.Play(*FindPoseProgram(ActionId::Left));

    for (int i = 0; i < 40 && player.IsPlaying(); i++)
    {
        advance(100);
        player.Update();
    }
    TEST_ASSERT_FALSE(player.IsPlaying());
    // 十步循环三次，其中第五步是四通道，其余九步是两通道
    TEST_ASSERT_EQUAL_INT(3 * (4 + 9 * 2), countWrites());
}

static void test_hand_left_returns_to_neutral_once()
{
    resetLog();
    RecordingServoWriter writer;
    PosePlayer player(writer);
    player.Play(*FindPoseProgram(ActionId::HandLeft));

    for (int i = 0; i < 20 && player.IsPlaying(); i++)
    {
        advance(100);
        player.Update();
    }
    TEST_ASSERT_FALSE(player.IsPlaying());

    int swingsDown = 0;
    int swingsUp   = 0;
    int neutral    = 0;
    for (const ServoWrite &w : gLog)
    {
        TEST_ASSERT_EQUAL_UINT8(2, w.channel); // 只动 Servo3
        if (w.angle == 0)
            swingsDown++;
        if (w.angle == 30)
            swingsUp++;
        if (w.angle == 90)
            neutral++;
    }
    TEST_ASSERT_EQUAL_INT(3, swingsDown);
    TEST_ASSERT_EQUAL_INT(3, swingsUp);
    TEST_ASSERT_EQUAL_INT(1, neutral); // 收尾归位只执行一次
}

int main(int, char **)
{
    UNITY_BEGIN();
    RUN_TEST(test_only_real_poses_have_programs);
    RUN_TEST(test_loop_metadata_matches_original);
    RUN_TEST(test_front_angles_match_original);
    RUN_TEST(test_sit_and_sleep_match_original);
    RUN_TEST(test_player_writes_first_step_immediately);
    RUN_TEST(test_player_ignores_hold_channels);
    RUN_TEST(test_player_finishes_after_last_step);
    RUN_TEST(test_player_repeats_left_three_times);
    RUN_TEST(test_hand_left_returns_to_neutral_once);
    return UNITY_END();
}
