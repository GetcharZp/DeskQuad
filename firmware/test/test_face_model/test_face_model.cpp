// 校验表情动画模型的参数收敛、眨眼、说话与粒子行为

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

    const FaceParticle *findParticle(const FaceState &state, ParticleKind kind)
    {
        for (uint8_t i = 0; i < state.particleCount; i++)
        {
            if (state.particles[i].kind == kind)
                return &state.particles[i];
        }
        return nullptr;
    }

} // namespace

static void test_happy_reaches_smile()
{
    Runner runner;
    runner.animator.SetExpression(EmojiId::Happy);
    runner.run(1500);

    const FaceParams &params = runner.animator.State().params;
    TEST_ASSERT_TRUE(params.mouthCurve > 0.5f);
    TEST_ASSERT_TRUE(params.eyeRound > 0.6f);
}

static void test_angry_reaches_frown_and_brows()
{
    Runner runner;
    runner.animator.SetExpression(EmojiId::Angry);
    runner.run(1500);

    const FaceParams &params = runner.animator.State().params;
    TEST_ASSERT_TRUE(params.mouthCurve < -0.4f);
    TEST_ASSERT_TRUE(params.browAngle < -0.4f);
}

static void test_dizzy_uses_cross_eyes()
{
    Runner runner;
    runner.animator.SetExpression(EmojiId::Dizzy);
    runner.run(1500);

    TEST_ASSERT_TRUE(runner.animator.State().params.eyeCross > 0.7f);
}

static void test_love_grows_blush_and_hearts()
{
    Runner runner;
    runner.animator.SetExpression(EmojiId::Love);
    runner.run(2500);

    const FaceState &state = runner.animator.State();
    TEST_ASSERT_TRUE(state.params.blush > 0.6f);
    TEST_ASSERT_TRUE(state.particleCount > 0);
    TEST_ASSERT_NOT_NULL(findParticle(state, ParticleKind::Heart));
}

static void test_sad_sheds_tears()
{
    Runner runner;
    runner.animator.SetExpression(EmojiId::Sad);
    runner.run(2500);

    TEST_ASSERT_NOT_NULL(findParticle(runner.animator.State(), ParticleKind::Tear));
}

static void test_blink_happens_over_time()
{
    Runner runner;
    runner.animator.SetExpression(EmojiId::Happy);
    runner.run(12000, 20);

    TEST_ASSERT_TRUE(runner.animator.BlinkCount() >= 1);
}

static void test_note_opens_mouth()
{
    Runner runner;
    FaceInputs inputs = {0.0f, 0.3f, 0.5f, 440, false};
    runner.animator.SetInputs(inputs);
    runner.run(120);

    TEST_ASSERT_TRUE(runner.animator.State().params.mouthOpen > 0.25f);
}

static void test_particle_count_is_bounded()
{
    Runner runner;
    runner.animator.SetExpression(EmojiId::Love);
    runner.run(20000, 16);

    TEST_ASSERT_TRUE(runner.animator.State().particleCount <= kMaxFaceParticles);
}

static void test_extreme_inputs_are_clamped()
{
    Runner runner;
    FaceInputs inputs = {5.0f, 5.0f, -3.0f, 4000, true};
    runner.animator.SetInputs(inputs);
    runner.animator.SetExpression(EmojiId::Error);
    runner.run(2000);

    const FaceParams &params = runner.animator.State().params;
    TEST_ASSERT_TRUE(params.eyeOpen >= 0.0f && params.eyeOpen <= 1.0f);
    TEST_ASSERT_TRUE(params.pupilSize >= 0.6f && params.pupilSize <= 1.4f);
    TEST_ASSERT_TRUE(params.browAngle >= -1.0f && params.browAngle <= 1.0f);
    TEST_ASSERT_TRUE(params.mouthOpen >= 0.0f && params.mouthOpen <= 1.0f);
    TEST_ASSERT_TRUE(params.mouthWidth >= 0.6f && params.mouthWidth <= 1.4f);
    TEST_ASSERT_TRUE(params.blush >= 0.0f && params.blush <= 1.0f);
}

static void test_hearts_fade_after_switching_expression()
{
    Runner runner;
    runner.animator.SetExpression(EmojiId::Love);
    runner.run(2000);
    TEST_ASSERT_NOT_NULL(findParticle(runner.animator.State(), ParticleKind::Heart));

    runner.animator.SetExpression(EmojiId::Happy);
    runner.run(4000);

    TEST_ASSERT_NULL(findParticle(runner.animator.State(), ParticleKind::Heart));
}

int main(int, char **)
{
    UNITY_BEGIN();
    RUN_TEST(test_happy_reaches_smile);
    RUN_TEST(test_angry_reaches_frown_and_brows);
    RUN_TEST(test_dizzy_uses_cross_eyes);
    RUN_TEST(test_love_grows_blush_and_hearts);
    RUN_TEST(test_sad_sheds_tears);
    RUN_TEST(test_blink_happens_over_time);
    RUN_TEST(test_note_opens_mouth);
    RUN_TEST(test_particle_count_is_bounded);
    RUN_TEST(test_extreme_inputs_are_clamped);
    RUN_TEST(test_hearts_fade_after_switching_expression);
    return UNITY_END();
}
