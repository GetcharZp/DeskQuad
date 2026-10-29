// pose_player.h 按步进序列驱动舵机
//
// 非阻塞：每一步只在时间到时才写入一次舵机，等待期间主循环继续运行，
// Web、语音和 AI 不会因为一个 3 秒的坐下动作而停摆。

#pragma once

#include <stdint.h>

#include <deskquad/core/pose_table.h>
#include <deskquad/core/servo_writer.h>

// PosePlayer 播放一个 PoseProgram
class PosePlayer
{
public:
    // 构造函数
    //
    // # Params:
    //
    //	writer: 舵机写入接口
    explicit PosePlayer(IServoWriter &writer);

    // Play 从头开始播放一个姿态序列，立即写入第一步
    void Play(const PoseProgram &program);

    // Update 推进播放进度，需在每次主循环中调用
    void Update();

    // Stop 中止当前播放，舵机保持在当前位置
    void Stop();

    // IsPlaying 判断是否仍在播放
    bool IsPlaying() const { return _playing; }

private:
    void applyStep(const PoseStep &step);

    IServoWriter &_writer;
    const PoseProgram *_program;
    uint8_t _stepIndex;
    uint8_t _loopLeft;
    unsigned long _stepStartMs;
    bool _playing;
};
