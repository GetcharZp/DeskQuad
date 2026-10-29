// motion_director.h 统一两套运动系统的出口
//
// DeskQuad 有两类动作：姿态动作（前进、坐下等固定序列）与情感动作
// （跳舞、点头等随情感强度变化的动作）。二者都通过这里下发，
// 仲裁规则只有一条：情感动作进行中时，姿态动作挂起等待。

#pragma once

#include <stdint.h>

#include <deskquad/core/pose_player.h>
#include <deskquad/core/servo_writer.h>
#include <deskquad/core/states.h>
#include <deskquad/motion/motion_emotion.h>

// MotionDirector 调度姿态动作与情感动作
class MotionDirector
{
public:
    // 构造函数
    //
    // # Params:
    //
    //	writer: 舵机写入接口，由 hal/servo_bus.h 实现
    explicit MotionDirector(IServoWriter &writer);

    // Begin 关联情感动作的舵机写入接口
    void Begin();

    // Update 推进动作进度，需在每次主循环中调用
    void Update();

    // RequestPose 请求执行一个姿态动作
    //
    // 只是登记意图，真正的执行时机由 TickPoses 决定。
    void RequestPose(ActionId id) { _pending = id; }

    // PendingPose 返回待执行的姿态编号
    ActionId PendingPose() const { return _pending; }

    // ClearPendingPose 丢弃待执行的姿态
    void ClearPendingPose() { _pending = ActionId::None; }

    // TickPoses 按仲裁规则启动待执行的姿态
    //
    // # Returns:
    //
    //	true 表示本帧启动了姿态
    bool TickPoses();

    // ConsumePoseFinished 取走一次「姿态播放完毕」事件
    //
    // 非阻塞化，完成时机必须显式上报，否则音效会提前。
    //
    // # Params:
    //
    //	doneMelody: 有完成事件时写入对应的提示音
    //
    // # Returns:
    //
    //	true 表示本次调用取走了一个完成事件
    bool ConsumePoseFinished(Melody &doneMelody);

    // TriggerEmotion 触发情感动作，立即覆盖当前姿态播放
    void TriggerEmotion(EmotionAction action, uint8_t repeat = 1);

    // SetEmotionalIntensity 用当前情感强度缩放情感动作
    void SetEmotionalIntensity(float pleasure, float arousal);

    bool IsPlayingPose() const { return _player.IsPlaying(); }
    bool IsPlayingEmotion() const { return _emotion.IsPlaying(); }
    bool IsBusy() const { return IsPlayingPose() || IsPlayingEmotion(); }

private:
    IServoWriter &_writer;
    PosePlayer _player;
    MotionEmotion _emotion;
    ActionId _pending;
    Melody _doneMelody; // 当前姿态完成后应播放的提示音
    bool _poseFinished; // 有待取走的完成事件
};
