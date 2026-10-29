// 校验串口命令解析

#include <unity.h>

#include <string.h>

#include <deskquad/core/serial_command_table.h>

void setUp() {}
void tearDown() {}

static void test_ask_carries_argument()
{
    const SerialCommand command = ParseSerialCommand("ask 你好吗");
    TEST_ASSERT_EQUAL(static_cast<int>(SerialCommandKind::Ask), static_cast<int>(command.kind));
    TEST_ASSERT_EQUAL_STRING("你好吗", command.argument);
}

static void test_longer_keyword_wins()
{
    const SerialCommand reset = ParseSerialCommand("emotion reset");
    TEST_ASSERT_EQUAL(static_cast<int>(SerialCommandKind::EmotionReset), static_cast<int>(reset.kind));

    const SerialCommand status = ParseSerialCommand("emotion");
    TEST_ASSERT_EQUAL(static_cast<int>(SerialCommandKind::EmotionStatus), static_cast<int>(status.kind));
}

static void test_agent_commands()
{
    TEST_ASSERT_EQUAL(static_cast<int>(SerialCommandKind::AgentClear),
                      static_cast<int>(ParseSerialCommand("ai clear").kind));
    TEST_ASSERT_EQUAL(static_cast<int>(SerialCommandKind::GestureTest),
                      static_cast<int>(ParseSerialCommand("gesture").kind));
}

static void test_pose_keywords()
{
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ActionId::Front),
                            static_cast<uint8_t>(ParseSerialCommand("front").pose));
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ActionId::Sleep),
                            static_cast<uint8_t>(ParseSerialCommand("sleep").pose));
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ActionId::HandLeft),
                            static_cast<uint8_t>(ParseSerialCommand("handleft").pose));
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ActionId::HandRight),
                            static_cast<uint8_t>(ParseSerialCommand("handright").pose));
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ActionId::Sit), static_cast<uint8_t>(ParseSerialCommand("sit").pose));
}

static void test_emoji_keywords()
{
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(EmojiId::Happy),
                            static_cast<uint8_t>(ParseSerialCommand("happy").emoji));
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(EmojiId::Dizzy),
                            static_cast<uint8_t>(ParseSerialCommand("dizzy").emoji));
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(EmojiId::Weather),
                            static_cast<uint8_t>(ParseSerialCommand("weather").emoji));
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(EmojiId::Clock),
                            static_cast<uint8_t>(ParseSerialCommand("clock").emoji));
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(EmojiId::Logo),
                            static_cast<uint8_t>(ParseSerialCommand("logo").emoji));
}

static void test_unknown_and_empty_are_none()
{
    TEST_ASSERT_EQUAL(static_cast<int>(SerialCommandKind::None), static_cast<int>(ParseSerialCommand("nonsense").kind));
    TEST_ASSERT_EQUAL(static_cast<int>(SerialCommandKind::None), static_cast<int>(ParseSerialCommand("").kind));
    TEST_ASSERT_EQUAL(static_cast<int>(SerialCommandKind::None), static_cast<int>(ParseSerialCommand(nullptr).kind));
    // 只有前缀不算 ask
    TEST_ASSERT_EQUAL(static_cast<int>(SerialCommandKind::None), static_cast<int>(ParseSerialCommand("ask").kind));
}

int main(int, char **)
{
    UNITY_BEGIN();
    RUN_TEST(test_ask_carries_argument);
    RUN_TEST(test_longer_keyword_wins);
    RUN_TEST(test_agent_commands);
    RUN_TEST(test_pose_keywords);
    RUN_TEST(test_emoji_keywords);
    RUN_TEST(test_unknown_and_empty_are_none);
    return UNITY_END();
}
