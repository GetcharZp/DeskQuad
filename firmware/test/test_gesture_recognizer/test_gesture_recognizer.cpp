// 校验手势识别的背景建模、轮廓分类与挥手时序

#include <unity.h>

#include <string.h>

#include <deskquad/perception/gesture_recognizer.h>

namespace
{

    constexpr int kWidth          = deskquad::kGestureFrameWidth;
    constexpr int kHeight         = deskquad::kGestureFrameHeight;
    constexpr uint8_t kBackground = 100;
    constexpr uint8_t kForeground = 200;

    void fillFrame(uint8_t *frame, uint8_t value)
    {
        memset(frame, value, deskquad::kGesturePixelCount);
    }

    void fillRect(uint8_t *frame, int x, int y, int width, int height)
    {
        for (int row = y; row < y + height; row++)
        {
            if (row < 0 || row >= kHeight)
                continue;
            for (int column = x; column < x + width; column++)
            {
                if (column < 0 || column >= kWidth)
                    continue;
                frame[row * kWidth + column] = kForeground;
            }
        }
    }

    void fillCircle(uint8_t *frame, int centerX, int centerY, int radius)
    {
        for (int row = centerY - radius; row <= centerY + radius; row++)
        {
            if (row < 0 || row >= kHeight)
                continue;
            for (int column = centerX - radius; column <= centerX + radius; column++)
            {
                if (column < 0 || column >= kWidth)
                    continue;
                const int dx = column - centerX;
                const int dy = row - centerY;
                if (dx * dx + dy * dy <= radius * radius)
                    frame[row * kWidth + column] = kForeground;
            }
        }
    }

    // learnBackground 用两帧空场景建立背景模型
    void learnBackground(GestureRecognizer &recognizer, uint8_t *frame)
    {
        fillFrame(frame, kBackground);
        recognizer.Classify(frame);
        recognizer.Classify(frame);
    }

    int gestureOf(const GestureResult &result)
    {
        return static_cast<int>(result.gesture);
    }

} // namespace

void setUp() {}
void tearDown() {}

static void test_first_frame_only_learns_background()
{
    GestureRecognizer recognizer;
    uint8_t frame[deskquad::kGesturePixelCount];

    fillFrame(frame, kBackground);
    const GestureResult result = recognizer.Classify(frame);

    TEST_ASSERT_EQUAL_INT(GESTURE_NONE, gestureOf(result));
    TEST_ASSERT_TRUE(recognizer.IsBackgroundReady());
}

static void test_empty_scene_is_none()
{
    GestureRecognizer recognizer;
    uint8_t frame[deskquad::kGesturePixelCount];
    learnBackground(recognizer, frame);

    const GestureResult result = recognizer.Classify(frame);

    TEST_ASSERT_EQUAL_INT(GESTURE_NONE, gestureOf(result));
    TEST_ASSERT_EQUAL_UINT16(0, result.blobArea);
}

static void test_fist_from_compact_blob()
{
    GestureRecognizer recognizer;
    uint8_t frame[deskquad::kGesturePixelCount];
    learnBackground(recognizer, frame);

    fillCircle(frame, 40, 36, 10);
    const GestureResult result = recognizer.Classify(frame);

    TEST_ASSERT_EQUAL_INT(GESTURE_FIST, gestureOf(result));
    TEST_ASSERT_TRUE(result.blobArea > 200);
    TEST_ASSERT_TRUE(result.confidence >= 0.55f);
}

static void test_palm_from_spread_fingers()
{
    GestureRecognizer recognizer;
    uint8_t frame[deskquad::kGesturePixelCount];
    learnBackground(recognizer, frame);

    fillRect(frame, 26, 40, 28, 14); // 掌心
    fillRect(frame, 30, 24, 4, 16);  // 食指
    fillRect(frame, 36, 22, 4, 18);  // 中指
    fillRect(frame, 42, 24, 4, 16);  // 无名指
    fillRect(frame, 48, 26, 4, 14);  // 小指

    const GestureResult result = recognizer.Classify(frame);

    TEST_ASSERT_EQUAL_INT(GESTURE_PALM, gestureOf(result));
    TEST_ASSERT_TRUE(result.fingerCount >= 3);
}

static void test_point_from_single_raised_finger()
{
    GestureRecognizer recognizer;
    uint8_t frame[deskquad::kGesturePixelCount];
    learnBackground(recognizer, frame);

    fillCircle(frame, 40, 45, 8);   // 拳头
    fillRect(frame, 39, 14, 4, 27); // 竖起的食指

    const GestureResult result = recognizer.Classify(frame);

    TEST_ASSERT_EQUAL_INT(GESTURE_POINT, gestureOf(result));
    TEST_ASSERT_TRUE(result.fingerCount <= 1);
}

static void test_wave_from_horizontal_oscillation()
{
    GestureRecognizer recognizer;
    uint8_t frame[deskquad::kGesturePixelCount];
    learnBackground(recognizer, frame);

    const int positions[] = {20, 34, 20, 34, 20};
    bool detected         = false;
    for (int i = 0; i < 5; i++)
    {
        fillFrame(frame, kBackground);
        fillCircle(frame, positions[i], 36, 10);
        if (recognizer.Classify(frame).gesture == GESTURE_WAVE)
            detected = true;
    }

    TEST_ASSERT_TRUE(detected);
}

static void test_static_hand_does_not_wave()
{
    GestureRecognizer recognizer;
    uint8_t frame[deskquad::kGesturePixelCount];
    learnBackground(recognizer, frame);

    for (int i = 0; i < 6; i++)
    {
        fillFrame(frame, kBackground);
        fillCircle(frame, 40, 36, 10);
        const GestureResult result = recognizer.Classify(frame);
        TEST_ASSERT_NOT_EQUAL(GESTURE_WAVE, gestureOf(result));
    }
}

static void test_reset_relearns_background()
{
    GestureRecognizer recognizer;
    uint8_t frame[deskquad::kGesturePixelCount];
    learnBackground(recognizer, frame);

    fillCircle(frame, 40, 36, 10);
    TEST_ASSERT_EQUAL_INT(GESTURE_FIST, gestureOf(recognizer.Classify(frame)));

    recognizer.Reset();
    TEST_ASSERT_FALSE(recognizer.IsBackgroundReady());

    fillFrame(frame, kBackground);
    const GestureResult result = recognizer.Classify(frame);
    TEST_ASSERT_EQUAL_INT(GESTURE_NONE, gestureOf(result));
    TEST_ASSERT_TRUE(recognizer.IsBackgroundReady());
}

static void test_null_frame_is_none()
{
    GestureRecognizer recognizer;
    const GestureResult result = recognizer.Classify(nullptr);

    TEST_ASSERT_EQUAL_INT(GESTURE_NONE, gestureOf(result));
    TEST_ASSERT_EQUAL_UINT16(0, result.blobArea);
}

int main(int, char **)
{
    UNITY_BEGIN();
    RUN_TEST(test_first_frame_only_learns_background);
    RUN_TEST(test_empty_scene_is_none);
    RUN_TEST(test_fist_from_compact_blob);
    RUN_TEST(test_palm_from_spread_fingers);
    RUN_TEST(test_point_from_single_raised_finger);
    RUN_TEST(test_wave_from_horizontal_oscillation);
    RUN_TEST(test_static_hand_does_not_wave);
    RUN_TEST(test_reset_relearns_background);
    RUN_TEST(test_null_frame_is_none);
    return UNITY_END();
}
