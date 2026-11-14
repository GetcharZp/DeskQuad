// interaction_status.h 描述屏幕上要提示的实时交互阶段
//
// 语音链路（豆包 ASR/TTS）与 AI 对话都会产出这里的状态，显示层只在
// 屏幕角落画一个小的状态胶囊，不影响时间、天气与表情的主体内容。

#pragma once

#include <stdint.h>

// InteractionKind 是一次交互当前所处的阶段
enum class InteractionKind : uint8_t
{
    None,      // 空闲，不显示任何提示
    Listening, // 正在听（ASR）
    Thinking,  // 正在请求/等待 AI 回复
    Speaking,  // 正在播报（TTS）
};

// InteractionStatus 是显示层绘制状态胶囊所需的最小信息
struct InteractionStatus
{
    InteractionKind kind; // 当前阶段
    uint8_t level;        // 0..100 的音频能量，驱动电平条
};
