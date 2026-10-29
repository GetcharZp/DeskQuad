// states.h 定义跨模块的状态编号
//
// 这一层不依赖 Arduino，可以被 env:native 直接编译测试。
// 编号是设备与 Web 页面、语音模块、LLM 之间的约定，不可随意调整。

#pragma once

#include <stdint.h>

// ActionId 是一次性姿态动作的编号，取值对应原 actionstate
enum class ActionId : uint8_t
{
    None        = 0,
    Front       = 1,  // 前进
    Left        = 2,  // 左转
    Right       = 3,  // 右转
    Back        = 4,  // 后退
    HandLeft    = 5,  // 抬左手
    HandRight   = 6,  // 抬右手
    Lie         = 7,  // 趴下
    Sit         = 8,  // 坐下
    RandomEmote = 9,  // 随机表情，由显示层处理，不是姿态
    Sleep       = 10, // 睡觉
};

// EmojiId 是 OLED 表情页编号，取值对应原 emojiState
enum class EmojiId : uint8_t
{
    Happy   = 0, // 开心
    Angry   = 1, // 生气
    Error   = 2, // 困惑/错误
    Curious = 3, // 好奇
    Love    = 4, // 喜爱
    Sad     = 5, // 难过
    Dizzy   = 6, // 晕
    Weather = 7, // 天气页
    Clock   = 8, // 时间页
    Logo    = 9, // 开机 Logo
};

// VoiceCommand 是 HLK-V20 输出的语音命令，编号与模块配置工具一致
enum class VoiceCommand : uint8_t
{
    None      = 0,  // 无新识别
    Forward   = 1,  // 前进
    Backward  = 2,  // 后退
    Left      = 3,  // 左转
    Right     = 4,  // 右转
    Stop      = 5,  // 停止
    Sit       = 6,  // 坐下
    Lie       = 7,  // 趴下
    Happy     = 8,  // 开心
    Angry     = 9,  // 生气
    Sad       = 10, // 难受
    Curious   = 11, // 好奇
    Love      = 12, // 喜欢
    Sleep     = 13, // 睡觉
    Free      = 14, // 自由模式
    HandLeft  = 15, // 抬左手
    HandRight = 16, // 抬右手
    Time      = 17, // 时间
    Weather   = 18, // 天气
    Logo      = 19, // 显示 Logo
    Hello     = 20, // 你好
    Bye       = 21, // 再见
    Error     = 22, // 识别无效
};

// EmotionAction 是情感动作编号，由 LLM 的 action 字段 11-18 触发
enum EmotionAction : uint8_t
{
    ACTION_NONE = 0,
    // 1-10 保留给 ActionId
    ACTION_DANCE     = 11, // 跳舞
    ACTION_NOD       = 12, // 点头
    ACTION_SHAKE     = 13, // 摇头
    ACTION_BOW       = 14, // 鞠躬
    ACTION_CELEBRATE = 15, // 庆祝
    ACTION_COMFORT   = 16, // 轻拍/安慰
    ACTION_STRETCH   = 17, // 伸懒腰
    ACTION_HIDE_FACE = 18, // 捂脸/害羞
};

// Melody 是内置旋律的编号，与 LLM 的 melody 字段取值一致
enum Melody : uint8_t
{
    MELODY_STARTUP,    // 开机
    MELODY_COMMAND_OK, // 命令确认
    MELODY_ERROR,      // 错误提示
    MELODY_HELLO,      // 打招呼
    MELODY_GOODBYE,    // 再见
    MELODY_HAPPY,      // 开心
    MELODY_SAD,        // 难过
    MELODY_ALERT,      // 警告
    MELODY_DONE,       // 完成
    MELODY_BEEP,       // 简单哔声
    MELODY_COUNT,      // 旋律总数，非旋律编号
};

// ActionDecision 描述一个 action 编号应当如何执行
struct ActionDecision
{
    enum class Kind : uint8_t
    {
        None,
        Pose,
        Emotion
    };

    Kind kind;             // 执行方式
    ActionId pose;         // kind == Pose 时有效
    EmotionAction emotion; // kind == Emotion 时有效
};

// DecideAction 把 LLM 返回的 action 编号解析为执行决策
//
// 1-10 走姿态，11-18 走情感动作，其余（含 0 与越界值）不执行任何动作。
ActionDecision DecideAction(int raw);

// IsRunnablePose 判断编号是否为可执行的姿态动作
bool IsRunnablePose(ActionId id);
