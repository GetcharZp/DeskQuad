// 多模态情感融合层
//
// 将语音命令、手势分类、文本输入归一化为统一的情感上下文，
// 送入 LLM。同时更新 PAD 情感状态。
//
// 架构: 多模态输入 → 情感上下文向量 → Affective Prompt
#pragma once

#include <Arduino.h>
#include <deskquad/emotion/emotion_engine.h>

// 输入模态类型
enum InputModality : uint8_t
{
    MOD_TEXT    = 0, // 文本输入 (Web/串口)
    MOD_VOICE   = 1, // 语音命令 (LD3320)
    MOD_GESTURE = 2, // 手势 (OV2640 + GestureRecognizer)
};

// 归一化多模态上下文
struct MultimodalContext
{
    InputModality modality;    // 输入模态
    String userText;           // 归一化文本
    int8_t gestureClass;       // -1 或无手势
    float gestureConfidence;   // 手势置信度
    int8_t voiceCmd;           // -1 或无语音
    String emotionDescription; // 当前情感描述 (用于 prompt)
};

class MultimodalFusion
{
public:
    MultimodalFusion();

    // 关联情感引擎
    void attachEmotionEngine(EmotionEngine *engine) { _emotion = engine; }

    // ---------- 处理各模态输入 ----------
    // 文本输入 (Web UI / 串口)
    MultimodalContext processText(const String &text);

    // 语音命令输入
    MultimodalContext processVoice(int8_t voiceCmd);

    // 手势输入
    MultimodalContext processGesture(int8_t gestureClass, float confidence);

    // ---------- 工具 ----------
    // 构建情感增强的 prompt 文本
    // 格式: "[情感:快乐 P=0.81 A=0.65 D=0.57][模态:语音] 用户说: 你好"
    String buildAffectivePrompt(const MultimodalContext &ctx);

    // 获取最近一次上下文
    const MultimodalContext &getLastContext() const { return _lastCtx; }

    // 设置调试
    void setDebug(bool debug) { _debug = debug; }

private:
    // process 三条输入通道的公共流程：更新情感、记录上下文、打印调试日志
    MultimodalContext process(InputModality modality, const String &text, int8_t gestureClass, int8_t voiceCmd,
                              float confidence);

    EmotionEngine *_emotion;
    MultimodalContext _lastCtx;
    bool _debug;

    // 语音命令 → 自然语言文本
    String _voiceCmdToText(int8_t cmd);
    // 手势 → 自然语言文本
    String _gestureToText(int8_t gestureClass);
    // 模态标签
    const char *_modalityLabel(InputModality mod);
};
