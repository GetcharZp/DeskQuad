// 校验姿态与情感动作的仲裁、完成事件与时间推进

#include <unity.h>

#include <deskquad/core/servo_writer.h>
#include <deskquad/core/states.h>
#include <deskquad/motion/motion_director.h>

// native 平台的时间源，定义在 src/platform/clock.cpp
void SetNowMs(unsigned long ms);
void AdvanceNowMs(unsigned long ms);

namespace
{

    // RecordingServoWriter 记录写入次数与各通道最后一次角度
    class RecordingServoWriter : public IServoWriter
    {
    public:
        int writes           = 0;
        int16_t lastAngle[4] = {90, 90, 90, 90};

        void Write(uint8_t channel, int16_t angle) override
        {
            if (channel >= 4)
                return;
            writes++;
            lastAngle[channel] = angle;
        }
    };

    // AdvanceUntil 以 20ms 步进推进时间，直到条件成立或超时
    template <typename Predicate> void AdvanceUntil(MotionDirector &director, unsigned long limitMs, Predicate done)
    {
        constexpr unsigned long kStepMs = 20;
        for (unsigned long elapsed = 0; elapsed <= limitMs; elapsed += kStepMs)
        {
            if (done())
                return;
            AdvanceNowMs(kStepMs);
            director.Update();
        }
    }

} // namespace

void setUp()
{
    SetNowMs(0);
}

void tearDown() {}

static void test_pose_reports_completion()
{
    RecordingServoWriter writer;
    MotionDirector director(writer);
    director.Begin();

    director.RequestPose(ActionId::Front);
    TEST_ASSERT_TRUE(director.TickPoses());
    TEST_ASSERT_TRUE(director.IsPlayingPose());
    TEST_ASSERT_TRUE(writer.writes > 0);

    Melody melody = MELODY_STARTUP;
    TEST_ASSERT_FALSE(director.ConsumePoseFinished(melody));

    AdvanceUntil(director, 5000, [&director]() { return !director.IsPlayingPose(); });

    TEST_ASSERT_FALSE(director.IsPlayingPose());
    TEST_ASSERT_TRUE(director.ConsumePoseFinished(melody));
    TEST_ASSERT_EQUAL(MELODY_BEEP, melody);
    // 完成事件只能取走一次
    TEST_ASSERT_FALSE(director.ConsumePoseFinished(melody));
}

static void test_emotion_blocks_pending_pose()
{
    RecordingServoWriter writer;
    MotionDirector director(writer);
    director.Begin();

    director.TriggerEmotion(ACTION_NOD);
    TEST_ASSERT_TRUE(director.IsPlayingEmotion());

    director.RequestPose(ActionId::Front);
    TEST_ASSERT_FALSE(director.TickPoses());
    TEST_ASSERT_TRUE(director.PendingPose() == ActionId::Front);

    AdvanceUntil(director, 5000, [&director]() { return !director.IsPlayingEmotion(); });

    TEST_ASSERT_FALSE(director.IsPlayingEmotion());
    TEST_ASSERT_TRUE(director.TickPoses());
    TEST_ASSERT_TRUE(director.PendingPose() == ActionId::None);
}

static void test_emotion_stops_current_pose()
{
    RecordingServoWriter writer;
    MotionDirector director(writer);
    director.Begin();

    director.RequestPose(ActionId::Front);
    TEST_ASSERT_TRUE(director.TickPoses());
    TEST_ASSERT_TRUE(director.IsPlayingPose());

    director.TriggerEmotion(ACTION_NOD);
    TEST_ASSERT_FALSE(director.IsPlayingPose());
    TEST_ASSERT_TRUE(director.IsPlayingEmotion());

    // 被打断的姿态不应再上报完成事件
    Melody melody = MELODY_STARTUP;
    director.Update();
    TEST_ASSERT_FALSE(director.ConsumePoseFinished(melody));
}

static void test_pending_pose_survives_until_emotion_ends()
{
    RecordingServoWriter writer;
    MotionDirector director(writer);
    director.Begin();

    director.RequestPose(ActionId::Sit);
    TEST_ASSERT_TRUE(director.TickPoses());
    TEST_ASSERT_TRUE(director.IsPlayingPose());

    // 姿态播放期间触发情感动作，随后重新登记同一个姿态
    director.TriggerEmotion(ACTION_NOD);
    director.RequestPose(ActionId::Sit);
    TEST_ASSERT_FALSE(director.TickPoses());

    AdvanceUntil(director, 5000, [&director]() { return !director.IsPlayingEmotion(); });
    TEST_ASSERT_TRUE(director.TickPoses());
    TEST_ASSERT_TRUE(director.IsPlayingPose());
}

int main(int, char **)
{
    UNITY_BEGIN();
    RUN_TEST(test_pose_reports_completion);
    RUN_TEST(test_emotion_blocks_pending_pose);
    RUN_TEST(test_emotion_stops_current_pose);
    RUN_TEST(test_pending_pose_survives_until_emotion_ends);
    return UNITY_END();
}
