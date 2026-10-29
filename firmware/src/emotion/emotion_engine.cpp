#include <deskquad/emotion/emotion_engine.h>

#include <math.h>

namespace
{
    // 情感锚点取自 Mehrabian (1996) 的基本情感 PAD 坐标，顺序与 AgentEmotion 一致
    struct EmotionAnchor
    {
        PADState pad;
        const char *label;
    };

    constexpr EmotionAnchor kAnchors[] = {
        {{0.00f, 0.30f, 0.50f}, "中性"},   // Neutral
        {{0.81f, 0.65f, 0.57f}, "快乐"},   // Happy
        {{-0.63f, 0.06f, -0.28f}, "悲伤"}, // Sad
        {{-0.51f, 0.59f, 0.25f}, "生气"},  // Angry
        {{0.40f, 0.67f, -0.13f}, "惊讶"},  // Surprise
        {{0.85f, 0.42f, 0.35f}, "喜爱"},   // Love
    };

    constexpr uint8_t kAnchorCount = sizeof(kAnchors) / sizeof(kAnchors[0]);
    static_assert(kAnchorCount == static_cast<uint8_t>(AgentEmotion::Love) + 1, "锚点表必须覆盖全部 AgentEmotion");

    // 手势对愉悦度的偏移，下标即手势类别 (1=挥手, 2=伸掌, 3=握拳, 4=指点)
    constexpr float kGesturePleasure[] = {0.00f, 0.20f, 0.10f, 0.10f, 0.30f};
    constexpr int8_t kGestureCount     = sizeof(kGesturePleasure) / sizeof(kGesturePleasure[0]);

    constexpr uint32_t kDecayIntervalMs = 1000;

    // personalityRate 把人格特质线性映射为速率倍率
    float personalityRate(float base, float trait, float span)
    {
        return base * (1.0f + span * (trait - 0.5f));
    }
} // namespace

void EmotionEngine::Update()
{
    const uint32_t now       = millis();
    const uint32_t elapsedMs = now - _lastUpdateMs;
    if (elapsedMs < kDecayIntervalMs)
        return;
    _lastUpdateMs = now;

    // 按实际经过的秒数折算保留比例，主循环卡顿不会让情感一次跳回基线
    const float seconds         = elapsedMs / 1000.0f;
    const float arousalBase     = _decayArousal + 0.02f * (_personality.neuroticism - 0.5f);
    const float arousalDecay    = constrain(arousalBase, 0.90f, 1.0f);
    const float retainPleasure  = powf(_decayPleasure, seconds);
    const float retainArousal   = powf(arousalDecay, seconds);
    const float retainDominance = powf(_decayDominance, seconds);

    _current.pleasure  = retainPleasure * _current.pleasure + (1.0f - retainPleasure) * _baseline.pleasure;
    _current.arousal   = retainArousal * _current.arousal + (1.0f - retainArousal) * _baseline.arousal;
    _current.dominance = retainDominance * _current.dominance + (1.0f - retainDominance) * _baseline.dominance;

    clampState();
    logState("衰减");
}

void EmotionEngine::Reset()
{
    _current = _baseline;
    Serial.println("[sys] emotion: reset to baseline");
}

void EmotionEngine::UpdateFromInput(const String &text, int8_t gesture, int8_t voice, float confidence)
{
    const float engagement = constrain(text.length() / 50.0f, 0.0f, 1.0f);

    // 目标值以基线为起点，各模态叠加偏移
    float targetP = _baseline.pleasure + 0.10f * (_personality.extraversion - 0.5f);
    float targetA = _baseline.arousal + 0.20f * engagement;
    float targetD = _baseline.dominance;

    if (gesture > 0 && gesture < kGestureCount)
    {
        const float weight = constrain(confidence, 0.0f, 1.0f);
        targetP += kGesturePleasure[gesture] * weight;
        targetA += 0.15f * weight;
    }

    // 语音指令给出明确的情感目标，优先级高于文本与手势的微调
    switch (static_cast<VoiceCommand>(voice))
    {
    case VoiceCommand::Happy:
    case VoiceCommand::Love:
    case VoiceCommand::Hello:
        targetP = 0.50f;
        targetA = 0.50f;
        break;
    case VoiceCommand::Angry:
    case VoiceCommand::Sad:
        targetP = -0.40f;
        targetA = 0.30f;
        break;
    default:
        break;
    }

    // 人格只调节变化速率：外倾加快积极反馈，宜人削弱负面反馈，开放放大激活反应
    float pleasureRate = personalityRate(0.30f, _personality.extraversion, 0.50f);
    if (targetP < _baseline.pleasure)
        pleasureRate *= 1.0f - 0.40f * (_personality.agreeableness - 0.5f);

    nudge(_current.pleasure, targetP, pleasureRate);
    nudge(_current.arousal, targetA, personalityRate(0.20f, _personality.openness, 0.40f));
    nudge(_current.dominance, targetD, personalityRate(0.10f, _personality.conscientiousness, 0.30f));

    clampState();
    logState("输入");
}

void EmotionEngine::UpdateFromEmotion(AgentEmotion emotion)
{
    const uint8_t index = static_cast<uint8_t>(emotion);
    if (index == 0 || index >= kAnchorCount)
        return; // 0 表示本次回复不改变情感

    const PADState &target = kAnchors[index].pad;
    nudge(_current.pleasure, target.pleasure, 0.50f);
    nudge(_current.arousal, target.arousal, 0.50f);
    nudge(_current.dominance, target.dominance, 0.30f);

    clampState();
    logState("LLM");
}

AgentEmotion EmotionEngine::GetEmotion() const
{
    uint8_t best   = 0;
    float bestDist = 0.0f;
    for (uint8_t i = 0; i < kAnchorCount; i++)
    {
        const float dp   = _current.pleasure - kAnchors[i].pad.pleasure;
        const float da   = _current.arousal - kAnchors[i].pad.arousal;
        const float dd   = _current.dominance - kAnchors[i].pad.dominance;
        const float dist = dp * dp + da * da + dd * dd;
        if (i == 0 || dist < bestDist)
        {
            bestDist = dist;
            best     = i;
        }
    }
    return static_cast<AgentEmotion>(best);
}

const char *EmotionEngine::GetLabel() const
{
    return kAnchors[static_cast<uint8_t>(GetEmotion())].label;
}

String EmotionEngine::Describe() const
{
    char buffer[96];
    snprintf(buffer, sizeof(buffer), "当前情感状态: %s (Pleasure=%.2f, Arousal=%.2f, Dominance=%.2f)", GetLabel(),
             _current.pleasure, _current.arousal, _current.dominance);
    return String(buffer);
}

void EmotionEngine::SetPersonality(const Personality &personality)
{
    _personality.extraversion      = constrain(personality.extraversion, 0.0f, 1.0f);
    _personality.agreeableness     = constrain(personality.agreeableness, 0.0f, 1.0f);
    _personality.conscientiousness = constrain(personality.conscientiousness, 0.0f, 1.0f);
    _personality.neuroticism       = constrain(personality.neuroticism, 0.0f, 1.0f);
    _personality.openness          = constrain(personality.openness, 0.0f, 1.0f);

    Serial.printf("[sys] emotion: personality E=%.1f A=%.1f C=%.1f N=%.1f O=%.1f\n", _personality.extraversion,
                  _personality.agreeableness, _personality.conscientiousness, _personality.neuroticism,
                  _personality.openness);
}

void EmotionEngine::SetDecay(float pleasure, float arousal, float dominance)
{
    _decayPleasure  = constrain(pleasure, 0.90f, 1.0f);
    _decayArousal   = constrain(arousal, 0.90f, 1.0f);
    _decayDominance = constrain(dominance, 0.90f, 1.0f);
}

void EmotionEngine::nudge(float &value, float target, float rate)
{
    value += rate * (target - value);
}

void EmotionEngine::clampState()
{
    _current.pleasure  = constrain(_current.pleasure, -1.0f, 1.0f);
    _current.arousal   = constrain(_current.arousal, 0.0f, 1.0f);
    _current.dominance = constrain(_current.dominance, 0.0f, 1.0f);
}

void EmotionEngine::logState(const char *event) const
{
    if (!_debug)
        return;
    Serial.printf("[sys] emotion: %s → P=%.2f A=%.2f D=%.2f [%s]\n", event, _current.pleasure, _current.arousal,
                  _current.dominance, GetLabel());
}
