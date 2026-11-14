// 校验眼睛动画模型的参数收敛、眨眼、视线、说话与微表情行为

#include <unity.h>

#include <deskquad/render/face_model.h>

void setUp() {}
void tearDown() {}

namespace
{

    struct Runner
    {
        FaceAnimator animator;
        uint32_t now = 0;

        Runner() { animator.Begin(0); }

        void step(uint32_t ms)
        {
            now += ms;
            animator.Update(now);
        }

        void run(uint32_t totalMs, uint32_t stepMs = 33)
        {
            for (uint32_t elapsed = 0; elapsed < totalMs; elapsed += stepMs)
                step(stepMs);
        }
    };

} // namespace

static void test_happy_squints_and_stays_big()
{
    Runner runner;
    runner.animator.SetExpression(EmojiId::Happy);
    runner.run(1500);

    const FaceParams &params = runner.animator.State().params;
    TEST_ASSERT_TRUE(params.eyeH > 11.0f);
    // 笑眼靠下眼睑上抬，而不是把眼睛闭成一条线
    TEST_ASSERT_TRUE(params.lidSquint > 0.2f);
    TEST_ASSERT_TRUE(params.eyeHeart < 0.1f);
}

static void test_gaze_wanders_when_idle()
{
    Runner runner;
    runner.animator.SetExpression(EmojiId::Happy);

    float minX = 0.0f;
    float maxX = 0.0f;
    for (int i = 0; i < 600; i++)
    {
        runner.step(33);
        const float pupilX = runner.animator.State().params.pupilX;
        if (pupilX < minX)
            minX = pupilX;
        if (pupilX > maxX)
            maxX = pupilX;
    }

    // 20 秒内视线至少扫过一段可察觉的范围
    TEST_ASSERT_TRUE(maxX - minX > 0.3f);
}

static void test_angry_narrows_and_slants()
{
    Runner runner;
    runner.animator.SetExpression(EmojiId::Angry);
    runner.run(1500);

    const FaceParams &params = runner.animator.State().params;
    TEST_ASSERT_TRUE(params.lidSlant > 0.5f);
    TEST_ASSERT_TRUE(params.eyeH < 13.0f);
}

static void test_sad_droops_and_looks_down()
{
    Runner runner;
    runner.animator.SetExpression(EmojiId::Sad);
    runner.run(1500);

    TEST_ASSERT_TRUE(runner.animator.State().params.lidSlant < -0.4f);

    // 视线会随机游移，用 10 秒均值判断“整体向下看”
    float sum = 0.0f;
    constexpr int kSamples = 300;
    for (int i = 0; i < kSamples; i++)
    {
        runner.step(33);
        sum += runner.animator.State().params.pupilY;
    }
    TEST_ASSERT_TRUE(sum / static_cast<float>(kSamples) > 0.35f);
}

static void test_error_is_cross_eyed()
{
    Runner runner;
    runner.animator.SetExpression(EmojiId::Error);
    runner.run(1500);

    const FaceParams &params = runner.animator.State().params;
    TEST_ASSERT_TRUE(params.pupilCross > 0.4f);
    TEST_ASSERT_TRUE(params.eyeAsym > 0.3f);
}

static void test_love_turns_into_heart_eyes()
{
    Runner runner;
    runner.animator.SetExpression(EmojiId::Love);
    runner.run(2000);

    TEST_ASSERT_TRUE(runner.animator.State().params.eyeHeart > 0.6f);
}

static void test_dizzy_uses_spiral_eyes()
{
    Runner runner;
    runner.animator.SetExpression(EmojiId::Dizzy);
    runner.run(1500);

    TEST_ASSERT_TRUE(runner.animator.State().params.eyeCross > 0.7f);
}

static void test_blink_happens_over_time()
{
    Runner runner;
    runner.animator.SetExpression(EmojiId::Happy);
    runner.run(12000, 20);

    TEST_ASSERT_TRUE(runner.animator.BlinkCount() >= 1);
}

static void test_note_animates_eyes()
{
    Runner runner;
    FaceInputs inputs = {0.0f, 0.3f, 0.5f, 440, false};
    runner.animator.SetInputs(inputs);
    runner.run(120);

    TEST_ASSERT_TRUE(runner.animator.State().params.talk > 0.2f);
}

static void test_extreme_inputs_are_clamped()
{
    Runner runner;
    FaceInputs inputs = {5.0f, 5.0f, -3.0f, 4000, true};
    runner.animator.SetInputs(inputs);
    runner.animator.SetExpression(EmojiId::Error);
    runner.run(2000);

    const FaceParams &params = runner.animator.State().params;
    TEST_ASSERT_TRUE(params.faceX >= -8.0f && params.faceX <= 8.0f);
    TEST_ASSERT_TRUE(params.faceY >= -8.0f && params.faceY <= 8.0f);
    TEST_ASSERT_TRUE(params.eyeW >= 6.0f && params.eyeW <= 17.0f);
    TEST_ASSERT_TRUE(params.eyeH >= 0.0f && params.eyeH <= 16.0f);
    TEST_ASSERT_TRUE(params.eyeTilt >= -1.0f && params.eyeTilt <= 1.0f);
    TEST_ASSERT_TRUE(params.lidSlant >= -1.0f && params.lidSlant <= 1.0f);
    TEST_ASSERT_TRUE(params.lidSquint >= 0.0f && params.lidSquint <= 1.0f);
    TEST_ASSERT_TRUE(params.pupilCross >= -1.0f && params.pupilCross <= 1.0f);
    TEST_ASSERT_TRUE(params.pupilSize >= 0.4f && params.pupilSize <= 1.5f);
    TEST_ASSERT_TRUE(params.eyeHeart >= 0.0f && params.eyeHeart <= 1.0f);
    TEST_ASSERT_TRUE(params.eyeCross >= 0.0f && params.eyeCross <= 1.0f);
    TEST_ASSERT_TRUE(params.talk >= 0.0f && params.talk <= 1.0f);
}

int main(int, char **)
{
    UNITY_BEGIN();
    RUN_TEST(test_happy_squints_and_stays_big);
    RUN_TEST(test_gaze_wanders_when_idle);
    RUN_TEST(test_angry_narrows_and_slants);
    RUN_TEST(test_sad_droops_and_looks_down);
    RUN_TEST(test_error_is_cross_eyed);
    RUN_TEST(test_love_turns_into_heart_eyes);
    RUN_TEST(test_dizzy_uses_spiral_eyes);
    RUN_TEST(test_blink_happens_over_time);
    RUN_TEST(test_note_animates_eyes);
    RUN_TEST(test_extreme_inputs_are_clamped);
    return UNITY_END();
}
