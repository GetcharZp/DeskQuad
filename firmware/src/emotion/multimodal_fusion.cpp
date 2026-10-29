// 多模态情感融合层实现
#include <deskquad/emotion/multimodal_fusion.h>

#include <deskquad/perception/gesture_recognizer.h>

namespace
{

    // 语音命令对应的自然语言文本，下标为 VoiceCommand 编号
    const char *kVoiceText[] = {
        "未知指令", // None
        "前进",     // Forward
        "后退",     // Backward
        "左转",     // Left
        "右转",     // Right
        "停止",     // Stop
        "坐下",     // Sit
        "趴下",     // Lie
        "开心",     // Happy
        "生气",     // Angry
        "难过",     // Sad
        "好奇",     // Curious
        "喜欢",     // Love
        "睡觉",     // Sleep
        "自由模式", // Free
        "抬左手",   // HandLeft
        "抬右手",   // HandRight
        "时间",     // Time
        "天气",     // Weather
        "Logo",     // Logo
        "你好",     // Hello
        "再见",     // Bye
        "未知指令", // Error
    };

    constexpr uint8_t kVoiceTextCount = sizeof(kVoiceText) / sizeof(kVoiceText[0]);
    static_assert(kVoiceTextCount == static_cast<uint8_t>(VoiceCommand::Error) + 1,
                  "语音文本表必须覆盖全部 VoiceCommand");

    // 手势对应的自然语言文本，下标为 GestureClass
    const char *kGestureText[]          = {"", "挥手", "伸掌", "握拳", "指点"};
    constexpr uint8_t kGestureTextCount = sizeof(kGestureText) / sizeof(kGestureText[0]);
    static_assert(kGestureTextCount == static_cast<uint8_t>(GESTURE_POINT) + 1, "手势文本表必须覆盖全部 GestureClass");

} // namespace

MultimodalFusion::MultimodalFusion()
{
    _emotion                   = nullptr;
    _debug                     = false;
    _lastCtx.modality          = MOD_TEXT;
    _lastCtx.gestureClass      = -1;
    _lastCtx.gestureConfidence = 0.0f;
    _lastCtx.voiceCmd          = -1;
}

const char *MultimodalFusion::_modalityLabel(InputModality mod)
{
    switch (mod)
    {
    case MOD_TEXT:
        return "文本";
    case MOD_VOICE:
        return "语音";
    case MOD_GESTURE:
        return "手势";
    default:
        return "未知";
    }
}

String MultimodalFusion::_voiceCmdToText(int8_t cmd)
{
    const uint8_t index = static_cast<uint8_t>(cmd);
    return index < kVoiceTextCount ? String(kVoiceText[index]) : String();
}

String MultimodalFusion::_gestureToText(int8_t gestureClass)
{
    const uint8_t index = static_cast<uint8_t>(gestureClass);
    return index < kGestureTextCount ? String(kGestureText[index]) : String();
}

MultimodalContext MultimodalFusion::process(InputModality modality, const String &text, int8_t gestureClass,
                                            int8_t voiceCmd, float confidence)
{
    MultimodalContext ctx;
    ctx.modality          = modality;
    ctx.userText          = text;
    ctx.gestureClass      = gestureClass;
    ctx.gestureConfidence = confidence;
    ctx.voiceCmd          = voiceCmd;

    if (_emotion)
    {
        _emotion->UpdateFromInput(text, gestureClass, voiceCmd, confidence);
        ctx.emotionDescription = _emotion->Describe();
    }

    _lastCtx = ctx;

    if (_debug)
    {
        Serial.printf("[sys] fusion: [%s] cmd=%d gesture=%d \"%s\" | %s\n", _modalityLabel(modality), voiceCmd,
                      gestureClass, text.c_str(), ctx.emotionDescription.c_str());
    }

    return ctx;
}

MultimodalContext MultimodalFusion::processText(const String &text)
{
    return process(MOD_TEXT, text, -1, -1, 1.0f);
}

MultimodalContext MultimodalFusion::processVoice(int8_t voiceCmd)
{
    return process(MOD_VOICE, _voiceCmdToText(voiceCmd), -1, voiceCmd, 1.0f);
}

MultimodalContext MultimodalFusion::processGesture(int8_t gestureClass, float confidence)
{
    return process(MOD_GESTURE, _gestureToText(gestureClass), gestureClass, -1, confidence);
}

String MultimodalFusion::buildAffectivePrompt(const MultimodalContext &ctx)
{
    String prompt;

    if (_emotion)
    {
        const PADState &pad = _emotion->GetState();
        prompt += "[情感状态:";
        prompt += _emotion->GetLabel();
        prompt += "|P=";
        prompt += String(pad.pleasure, 2);
        prompt += "|A=";
        prompt += String(pad.arousal, 2);
        prompt += "|D=";
        prompt += String(pad.dominance, 2);
        prompt += "]";
    }

    prompt += "[模态:";
    prompt += _modalityLabel(ctx.modality);
    prompt += "]";

    if (ctx.gestureClass > 0)
    {
        prompt += "[手势:";
        prompt += _gestureToText(ctx.gestureClass);
        prompt += "]";
    }

    prompt += " 用户说: \"";
    prompt += ctx.userText;
    prompt += "\"";

    return prompt;
}
