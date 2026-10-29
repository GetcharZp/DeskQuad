// 情感动作模块实现
// 支持步进序列 + 参数化正弦波动作生成

#include <deskquad/motion/motion_emotion.h>

#include <math.h>

#include <deskquad/core/clock.h>

#ifdef ARDUINO
#include <Arduino.h>
#endif

namespace
{

    // 触发日志只在设备上输出；native 测试构建下是空实现
    void logTrigger(bool debug, bool parametric, int action, float intensity, float speed)
    {
#ifdef ARDUINO
        if (!debug)
            return;
        if (parametric)
        {
            Serial.printf("[sys] motion: Parametric action %d (intensity=%.1f, speed=%.1f)\n", action, intensity,
                          speed);
        }
        else
        {
            Serial.printf("[sys] motion: Step action %d\n", action);
        }
#else
        (void)debug;
        (void)parametric;
        (void)action;
        (void)intensity;
        (void)speed;
#endif
    }

} // namespace

// 步进序列 (离散动作)
const MotionEmotion::MotionStep MotionEmotion::_bowSteps[] = {
    {90, 90, 90, 90, 200},   {40, 40, 140, 140, 500}, // 坐下
    {130, 130, 90, 90, 400},                          // 前倾
    {130, 130, 90, 90, 500},                          // 保持
    {40, 40, 140, 140, 400},                          // 回正
    {90, 90, 90, 90, 400},                            // 站起
    {0, 0, 0, 0, 0},
};

const MotionEmotion::MotionStep MotionEmotion::_stretchSteps[] = {
    {140, 140, 90, 90, 400}, {90, 90, 40, 40, 400}, {140, 140, 40, 40, 500}, {90, 90, 90, 90, 400}, {0, 0, 0, 0, 0},
};

const MotionEmotion::MotionStep MotionEmotion::_hideFaceSteps[] = {
    {140, 140, 90, 90, 400}, {130, 130, 70, 70, 300}, {140, 140, 90, 90, 500}, {90, 90, 90, 90, 400}, {0, 0, 0, 0, 0},
};

// 参数化动作预设：每个舵机有独立的振幅比例、频率偏移与相位
// 下标为 EmotionAction 编号，nullptr 表示该编号不是参数化动作
namespace
{

    const OscillatorPreset kDance = {
        30.0f, 2.0f, 2000.0f, {1.0f, 0.7f, 0.5f, 0.7f}, {0.0f, 0.3f, 0.6f, 0.9f}, {0.0f, 1.57f, 3.14f, 4.71f}};
    const OscillatorPreset kNod = {
        20.0f, 1.5f, 1500.0f, {0.5f, 0.5f, -0.5f, -0.5f}, {0.0f, 0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f, 0.0f}};
    const OscillatorPreset kShake = {
        25.0f, 3.0f, 1200.0f, {1.0f, -1.0f, -1.0f, 1.0f}, {0.0f, 0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 3.14f, 3.14f}};
    const OscillatorPreset kCelebrate = {
        40.0f, 4.0f, 1500.0f, {1.0f, 1.0f, 0.3f, 0.3f}, {0.0f, 0.0f, 0.0f, 0.0f}, {0.0f, 3.14f, 0.0f, 3.14f}};
    const OscillatorPreset kComfort = {
        15.0f, 0.8f, 3000.0f, {1.0f, -1.0f, 1.0f, -1.0f}, {0.0f, 0.0f, 0.0f, 0.0f}, {0.0f, 1.57f, 3.14f, 4.71f}};

    const OscillatorPreset *const kPresets[] = {
        nullptr,     // ACTION_NONE
        nullptr,     // 1
        nullptr,     // 2
        nullptr,     // 3
        nullptr,     // 4
        nullptr,     // 5
        nullptr,     // 6
        nullptr,     // 7
        nullptr,     // 8
        nullptr,     // 9
        nullptr,     // 10
        &kDance,     // ACTION_DANCE
        &kNod,       // ACTION_NOD
        &kShake,     // ACTION_SHAKE
        nullptr,     // ACTION_BOW，步进序列
        &kCelebrate, // ACTION_CELEBRATE
        &kComfort,   // ACTION_COMFORT
        nullptr,     // ACTION_STRETCH，步进序列
        nullptr,     // ACTION_HIDE_FACE，步进序列
    };

    static_assert(sizeof(kPresets) / sizeof(kPresets[0]) == static_cast<unsigned>(ACTION_HIDE_FACE) + 1,
                  "预设表必须覆盖全部 EmotionAction");

} // namespace

OscillatorParams MotionEmotion::makePreset(const OscillatorPreset &preset, float intensity, float speed)
{
    OscillatorParams p;
    p.durationMs = static_cast<uint16_t>(preset.durationMs / speed);
    for (uint8_t i = 0; i < 4; i++)
    {
        p.center[i]    = 90;
        p.amplitude[i] = static_cast<int16_t>(preset.amplitude * intensity * preset.amplitudeRatio[i]);
        p.frequency[i] = preset.frequency * speed + preset.frequencyOffset[i];
        p.phase[i]     = preset.phase[i];
    }
    return p;
}

// 构造函数
MotionEmotion::MotionEmotion()
{
    _writer         = nullptr;
    _attached       = false;
    _debug          = false;
    _isPlaying      = false;
    _currentAction  = ACTION_NONE;
    _repeatLeft     = 0;
    _stepStartMs    = 0;
    _stepIndex      = 0;
    _parametricMode = false;
    _oscStartMs     = 0;
    _intensityScale = 1.0f;
    _speedScale     = 1.0f;
    // 初始化振荡器
    for (int i = 0; i < 4; i++)
    {
        _osc.center[i]    = 90;
        _osc.amplitude[i] = 0;
        _osc.frequency[i] = 1.0f;
        _osc.phase[i]     = 0;
    }
    _osc.durationMs = 1000;
}

// 关联舵机
void MotionEmotion::Attach(IServoWriter &writer)
{
    _writer   = &writer;
    _attached = true;
}

// 触发情感动作
void MotionEmotion::Trigger(EmotionAction action, uint8_t repeat)
{
    if (!_attached)
        return;
    if (repeat == 0)
        repeat = 1;

    _isPlaying      = false; // 停止当前
    _parametricMode = false;

    // 根据动作类型选择：参数化（查预设表）或步进序列（查步进表）
    const uint8_t index            = static_cast<uint8_t>(action);
    const OscillatorPreset *preset = index < sizeof(kPresets) / sizeof(kPresets[0]) ? kPresets[index] : nullptr;
    uint8_t stepCount              = 0;
    const MotionStep *steps        = getSteps(action, stepCount);

    if (preset != nullptr)
    {
        _osc            = makePreset(*preset, _intensityScale, _speedScale);
        _parametricMode = true;
    }
    else if (steps == nullptr)
    {
        return;
    }

    _currentAction = action;
    _repeatLeft    = repeat;
    _stepIndex     = 0;
    _stepStartMs   = NowMs();
    _oscStartMs    = NowMs();
    _isPlaying     = true;

    logTrigger(_debug, _parametricMode, action, _intensityScale, _speedScale);
}

// 触发参数化动作
void MotionEmotion::TriggerParametric(const OscillatorParams &params, uint8_t repeat)
{
    if (!_attached)
        return;
    _isPlaying      = false;
    _osc            = params;
    _parametricMode = true;
    _currentAction  = ACTION_DANCE; // 通用标记
    _repeatLeft     = (repeat > 0) ? repeat : 1;
    _oscStartMs = _stepStartMs = NowMs();
    _stepIndex                 = 0;
    _isPlaying                 = true;
}

// 设置情感强度
void MotionEmotion::SetEmotionalIntensity(float pleasure, float arousal)
{
    // pleasure [-1,1] → intensity [0.5, 1.5]
    // arousal [0,1] → speed [0.5, 1.5]
    _intensityScale = 0.5f + (pleasure + 1.0f) * 0.5f;
    if (_intensityScale < 0.5f)
        _intensityScale = 0.5f;
    if (_intensityScale > 1.5f)
        _intensityScale = 1.5f;

    _speedScale = 0.5f + arousal;
    if (_speedScale < 0.5f)
        _speedScale = 0.5f;
    if (_speedScale > 1.5f)
        _speedScale = 1.5f;
}

// 停止
void MotionEmotion::Stop()
{
    _isPlaying      = false;
    _parametricMode = false;
    _currentAction  = ACTION_NONE;
    _repeatLeft     = 0;
}

// 应用步进
void MotionEmotion::applyStep(const MotionStep &step)
{
    if (!_attached)
        return;
    if (step.pos1 >= 0 && step.pos1 <= 180)
        _writer->Write(0, step.pos1);
    if (step.pos2 >= 0 && step.pos2 <= 180)
        _writer->Write(1, step.pos2);
    if (step.pos3 >= 0 && step.pos3 <= 180)
        _writer->Write(2, step.pos3);
    if (step.pos4 >= 0 && step.pos4 <= 180)
        _writer->Write(3, step.pos4);
}

// 应用振荡器
void MotionEmotion::applyOscillator(unsigned long elapsedMs)
{
    if (!_attached)
        return;
    float t = elapsedMs / 1000.0f; // 秒
    for (int i = 0; i < 4; i++)
    {
        float angle =
            _osc.center[i] + _osc.amplitude[i] * sinf(2.0f * 3.14159f * _osc.frequency[i] * t + _osc.phase[i]);
        // 限幅
        if (angle < 0)
            angle = 0;
        if (angle > 180)
            angle = 180;
        _writer->Write(i, (uint16_t)angle);
    }
}

// 获取步进序列
const MotionEmotion::MotionStep *MotionEmotion::getSteps(EmotionAction action, uint8_t &count) const
{
    const MotionStep *steps = nullptr;
    count                   = 0;
    switch (action)
    {
    case ACTION_BOW:
        steps = _bowSteps;
        break;
    case ACTION_STRETCH:
        steps = _stretchSteps;
        break;
    case ACTION_HIDE_FACE:
        steps = _hideFaceSteps;
        break;
    default:
        return nullptr;
    }
    while (steps[count].durationMs > 0)
        count++;
    return steps;
}

// 主循环
void MotionEmotion::Update()
{
    if (!_isPlaying || !_attached)
        return;

    unsigned long now = NowMs();

    if (_parametricMode)
    {
        // 参数化模式：持续更新正弦振荡
        unsigned long elapsed = now - _oscStartMs;
        if (elapsed >= _osc.durationMs)
        {
            // 完成一个周期
            if (_repeatLeft > 1)
            {
                _repeatLeft--;
                _oscStartMs = now;
            }
            else
            {
                _isPlaying = false;
                // 回到中立位置
                for (uint8_t i = 0; i < 4; i++)
                    _writer->Write(i, 90);
                return;
            }
        }
        applyOscillator(elapsed);
    }
    else
    {
        // 步进模式
        uint8_t stepCount       = 0;
        const MotionStep *steps = getSteps(_currentAction, stepCount);
        if (!steps || stepCount == 0)
        {
            _isPlaying = false;
            return;
        }

        unsigned long elapsed = now - _stepStartMs;
        if (_stepIndex < stepCount && elapsed >= steps[_stepIndex].durationMs)
        {
            _stepIndex++;
            if (_stepIndex >= stepCount || steps[_stepIndex].durationMs == 0)
            {
                if (_repeatLeft > 1)
                {
                    _repeatLeft--;
                    _stepIndex   = 0;
                    _stepStartMs = now;
                    applyStep(steps[0]);
                }
                else
                {
                    _isPlaying = false;
                }
            }
            else
            {
                _stepStartMs = now;
                applyStep(steps[_stepIndex]);
            }
        }
    }
}
