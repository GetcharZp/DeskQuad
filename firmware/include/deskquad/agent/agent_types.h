// agent_types.h 定义 AI Agent 的协议结构
//
// 数字取值是设备与 LLM 输出约定，改动需同步 agent_content.h 中的提示词。
// EmotionAction 属于运动概念，定义在 core/states.h。

#pragma once

#include <Arduino.h>
#include <deskquad/core/states.h>

// AgentEmotion 是 LLM 回复中 emotion 字段的取值，Neutral 表示情感不变
enum class AgentEmotion : uint8_t
{
    Neutral  = 0,
    Happy    = 1,
    Sad      = 2,
    Angry    = 3,
    Surprise = 4,
    Love     = 5,
};

enum AgentState : uint8_t
{
    AGENT_IDLE = 0,
    AGENT_BUSY = 1,
};

// AgentResponse 是一次 LLM 回复解析后的结构化结果，负值表示该项无变化
struct AgentResponse
{
    String reply_text; // 回复文本，供 Web UI 显示
    int8_t action;     // -1=无变化，0-10=普通姿态，11+=EmotionAction
    int8_t expression; // -1=无变化，0-9=表情编号
    int8_t melody;     // -1=无变化，0-9=Melody
    uint8_t repeat;    // 动作重复次数
    int8_t emotion;    // 0=无变化，1-5=AgentEmotion
};

// ConversationEntry 是对话历史的一轮
struct ConversationEntry
{
    String user;      // 用户输入，截断至约 100 字符
    String assistant; // AI 回复，截断至约 200 字符
};
