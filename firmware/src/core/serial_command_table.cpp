#include <deskquad/core/serial_command_table.h>

#include <string.h>

namespace
{

    // KeywordCommand 描述一条无参数命令
    struct KeywordCommand
    {
        const char *keyword;
        SerialCommandKind kind;
        ActionId pose;
        EmojiId emoji;
    };

    // 顺序敏感：「emotion reset」必须排在「emotion」之前
    const KeywordCommand kKeywords[] = {
        {"ai clear", SerialCommandKind::AgentClear, ActionId::None, EmojiId::Happy},
        {"emotion reset", SerialCommandKind::EmotionReset, ActionId::None, EmojiId::Happy},
        {"emotion", SerialCommandKind::EmotionStatus, ActionId::None, EmojiId::Happy},
        {"gesture", SerialCommandKind::GestureTest, ActionId::None, EmojiId::Happy},

        {"front", SerialCommandKind::Pose, ActionId::Front, EmojiId::Happy},
        {"back", SerialCommandKind::Pose, ActionId::Back, EmojiId::Happy},
        {"left", SerialCommandKind::Pose, ActionId::Left, EmojiId::Happy},
        {"right", SerialCommandKind::Pose, ActionId::Right, EmojiId::Happy},
        {"handleft", SerialCommandKind::Pose, ActionId::HandLeft, EmojiId::Happy},
        {"handright", SerialCommandKind::Pose, ActionId::HandRight, EmojiId::Happy},
        {"sit", SerialCommandKind::Pose, ActionId::Sit, EmojiId::Happy},
        {"lie", SerialCommandKind::Pose, ActionId::Lie, EmojiId::Happy},
        {"sleep", SerialCommandKind::Pose, ActionId::Sleep, EmojiId::Happy},

        // 表情关键字与 EmojiId 枚举名一一对应（小写）
        {"happy", SerialCommandKind::Emoji, ActionId::None, EmojiId::Happy},
        {"angry", SerialCommandKind::Emoji, ActionId::None, EmojiId::Angry},
        {"error", SerialCommandKind::Emoji, ActionId::None, EmojiId::Error},
        {"curious", SerialCommandKind::Emoji, ActionId::None, EmojiId::Curious},
        {"love", SerialCommandKind::Emoji, ActionId::None, EmojiId::Love},
        {"sad", SerialCommandKind::Emoji, ActionId::None, EmojiId::Sad},
        {"dizzy", SerialCommandKind::Emoji, ActionId::None, EmojiId::Dizzy},
        {"weather", SerialCommandKind::Emoji, ActionId::None, EmojiId::Weather},
        {"clock", SerialCommandKind::Emoji, ActionId::None, EmojiId::Clock},
        {"logo", SerialCommandKind::Emoji, ActionId::None, EmojiId::Logo},
    };

    constexpr uint8_t kKeywordCount = sizeof(kKeywords) / sizeof(kKeywords[0]);

    const char *kAskPrefix = "ask ";

} // namespace

SerialCommand ParseSerialCommand(const char *line)
{
    SerialCommand command{SerialCommandKind::None, ActionId::None, EmojiId::Happy, nullptr};
    if (line == nullptr || line[0] == '\0')
        return command;

    if (strncmp(line, kAskPrefix, strlen(kAskPrefix)) == 0)
    {
        command.kind     = SerialCommandKind::Ask;
        command.argument = line + strlen(kAskPrefix);
        return command;
    }

    for (uint8_t i = 0; i < kKeywordCount; i++)
    {
        if (strcmp(line, kKeywords[i].keyword) != 0)
            continue;
        command.kind  = kKeywords[i].kind;
        command.pose  = kKeywords[i].pose;
        command.emoji = kKeywords[i].emoji;
        return command;
    }
    return command;
}
