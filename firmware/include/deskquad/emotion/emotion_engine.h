// emotion_engine.h 用 PAD 模型维护机器人的情感状态
//
// PAD 是 Mehrabian 的 pleasure-arousal-dominance 模型：pleasure 取值
// [-1, 1]，arousal 与 dominance 取值 [0, 1]。外部输入把状态推向对应
// 锚点，没有输入时按指数衰减回到基线；离散情感由最近邻锚点决定。
// Big Five 人格只影响变化速率与消退快慢，不改变锚点位置。
#pragma once

#include <Arduino.h>

#include <deskquad/agent/agent_types.h>

// PADState 是情感的连续状态
struct PADState
{
    float pleasure;  // 愉悦度 [-1, 1]
    float arousal;   // 激活度 [0, 1]
    float dominance; // 支配度 [0, 1]
};

// Personality 是 Big Five 人格参数，各维度 [0, 1]
struct Personality
{
    float extraversion;      // 外倾性 — 积极情感上升更快
    float agreeableness;     // 宜人性 — 负面情感更难被激发
    float conscientiousness; // 尽责性 — 支配感变化更克制
    float neuroticism;       // 神经质 — 情感消退更慢
    float openness;          // 开放性 — 激活反应更强
};

// EmotionEngine 把文本、手势、语音与 LLM 反馈归约为统一情感状态
class EmotionEngine
{
public:
    // Update 每帧调用，按实际经过时间向基线衰减
    void Update();

    // Reset 重置到基线
    void Reset();

    // UpdateFromInput 从一次用户输入更新情感
    //
    // # Params:
    //
    //	text: 归一化后的输入文本
    //	gesture: 手势类别，-1 表示无手势
    //	voice: 语音指令编号，-1 表示无语音
    //	confidence: 手势置信度 [0, 1]，非手势输入忽略
    void UpdateFromInput(const String &text, int8_t gesture = -1, int8_t voice = -1, float confidence = 1.0f);

    // UpdateFromEmotion 从 LLM 回复的 emotion 字段更新情感
    void UpdateFromEmotion(AgentEmotion emotion);

    // GetState 返回当前 PAD 状态
    const PADState &GetState() const { return _current; }

    // GetEmotion 把当前状态映射为最近的离散情感
    AgentEmotion GetEmotion() const;

    // GetLabel 返回离散情感的中文名称
    const char *GetLabel() const;

    // Describe 返回注入 LLM 提示词的情感描述
    String Describe() const;

    // SetPersonality 设置 Big Five 人格
    void SetPersonality(const Personality &personality);

    // SetDecay 设置每秒保留比例 [0.9, 1.0]，越小回归基线越快
    void SetDecay(float pleasure, float arousal, float dominance);

    // SetDebug 打印每次状态变化
    void SetDebug(bool debug) { _debug = debug; }

private:
    void nudge(float &value, float target, float rate);
    void clampState();
    void logState(const char *event) const;

    Personality _personality = {0.70f, 0.80f, 0.60f, 0.30f, 0.70f};
    PADState _baseline       = {0.00f, 0.30f, 0.50f};
    PADState _current        = _baseline;

    float _decayPleasure  = 0.97f;
    float _decayArousal   = 0.98f;
    float _decayDominance = 0.99f;

    uint32_t _lastUpdateMs = 0;
    bool _debug            = false;
};
