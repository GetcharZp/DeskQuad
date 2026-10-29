// motion_emotion.h 定义情感动作
//
// 支持两种生成方式：步进序列（鞠躬、伸懒腰、捂脸）与参数化正弦波
// （跳舞、点头、摇头、庆祝、轻拍）。参数化方式让动作幅度和速度随
// 情感强度连续变化，比固定序列更有生气。

#pragma once

#include <stdint.h>

#include <deskquad/core/servo_writer.h>
#include <deskquad/core/states.h>

// OscillatorParams 描述一组正弦振荡
struct OscillatorParams
{
    int16_t center[4];    // 各舵机中心角度 0-180
    int16_t amplitude[4]; // 各舵机振幅 0-90
    float frequency[4];   // 各舵机频率 (Hz)
    float phase[4];       // 各舵机相位偏移 (rad)
    uint16_t durationMs;  // 单次持续时长
};

// OscillatorPreset 是一个参数化动作的波形模板，播放时按情感强度与速度缩放
struct OscillatorPreset
{
    float amplitude;          // 基准振幅 (度)，乘以强度得到实际振幅
    float frequency;          // 基准频率 (Hz)，乘以速度得到实际频率
    float durationMs;         // 基准时长 (ms)，除以速度得到实际时长
    float amplitudeRatio[4];  // 各舵机振幅相对基准的比例
    float frequencyOffset[4]; // 各舵机频率在缩放后的附加偏移 (Hz)
    float phase[4];           // 各舵机相位 (rad)
};

// MotionEmotion 播放情感动作
class MotionEmotion
{
public:
    MotionEmotion();

    // Attach 关联舵机写入接口
    void Attach(IServoWriter &writer);

    // Trigger 触发情感动作
    //
    // # Params:
    //
    //	action: 情感动作编号
    //	repeat: 重复次数，0 视为 1
    void Trigger(EmotionAction action, uint8_t repeat = 1);

    // TriggerParametric 触发自定义的参数化动作
    void TriggerParametric(const OscillatorParams &params, uint8_t repeat = 1);

    // Update 推进播放进度
    void Update();

    // Stop 中止当前动作
    void Stop();

    bool IsPlaying() const { return _isPlaying; }
    bool IsAttached() const { return _attached; }

    // SetEmotionalIntensity 用当前情感强度缩放动作幅度与速度
    //
    // # Params:
    //
    //	pleasure: 愉悦度 -1.0-1.0，映射到幅度缩放 0.5-1.5
    //	arousal:  激活度 0.0-1.0，映射到速度缩放 0.5-1.5
    void SetEmotionalIntensity(float pleasure, float arousal);

    void SetDebug(bool debug) { _debug = debug; }

private:
    IServoWriter *_writer;
    bool _attached;
    bool _isPlaying;
    bool _debug;

    EmotionAction _currentAction;
    uint8_t _repeatLeft;
    unsigned long _stepStartMs;
    uint8_t _stepIndex;
    bool _parametricMode; // true=参数化正弦，false=步进序列

    OscillatorParams _osc;
    unsigned long _oscStartMs;

    float _intensityScale; // 幅度缩放 0.5-1.5
    float _speedScale;     // 速度缩放 0.5-1.5

    // MotionStep 是步进序列中的一步
    struct MotionStep
    {
        int16_t pos1, pos2, pos3, pos4;
        uint16_t durationMs;
    };

    static const MotionStep _bowSteps[];
    static const MotionStep _stretchSteps[];
    static const MotionStep _hideFaceSteps[];

    static OscillatorParams makePreset(const OscillatorPreset &preset, float intensity, float speed);

    const MotionStep *getSteps(EmotionAction action, uint8_t &count) const;
    void applyStep(const MotionStep &step);
    void applyOscillator(unsigned long elapsedMs);
};
