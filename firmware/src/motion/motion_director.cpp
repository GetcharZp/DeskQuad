#include <deskquad/motion/motion_director.h>

namespace
{

    // 姿态动作完成后播放的提示音，索引为 ActionId
    const Melody kDoneMelody[] = {
        MELODY_BEEP, // None，不会用到
        MELODY_BEEP, // Front
        MELODY_BEEP, // Left
        MELODY_BEEP, // Right
        MELODY_BEEP, // Back
        MELODY_BEEP, // HandLeft
        MELODY_BEEP, // HandRight
        MELODY_DONE, // Lie
        MELODY_DONE, // Sit
        MELODY_BEEP, // RandomEmote，由显示层处理，不会走到这里
        MELODY_DONE, // Sleep
    };

    // 下标为 ActionId，表必须覆盖到 Sleep
    static_assert(sizeof(kDoneMelody) / sizeof(kDoneMelody[0]) == static_cast<unsigned>(ActionId::Sleep) + 1,
                  "提示音表必须覆盖全部 ActionId");

} // namespace

MotionDirector::MotionDirector(IServoWriter &writer)
    : _writer(writer), _player(writer), _pending(ActionId::None), _doneMelody(MELODY_BEEP), _poseFinished(false)
{
}

void MotionDirector::Begin()
{
    _emotion.Attach(_writer);
}

void MotionDirector::Update()
{
    const bool wasPlaying = _player.IsPlaying();
    _player.Update();
    if (wasPlaying && !_player.IsPlaying())
    {
        _poseFinished = true;
    }
    _emotion.Update();
}

bool MotionDirector::TickPoses()
{
    if (_pending == ActionId::None)
        return false;
    if (!IsRunnablePose(_pending))
        return false;
    // 情感动作优先，姿态继续挂起
    if (_emotion.IsPlaying())
        return false;

    const PoseProgram *program = FindPoseProgram(_pending);
    if (program == nullptr)
        return false;

    _doneMelody = kDoneMelody[static_cast<uint8_t>(_pending)];
    _player.Play(*program);
    _pending = ActionId::None;
    return true;
}

bool MotionDirector::ConsumePoseFinished(Melody &doneMelody)
{
    if (!_poseFinished)
        return false;
    _poseFinished = false;
    doneMelody    = _doneMelody;
    return true;
}

void MotionDirector::TriggerEmotion(EmotionAction action, uint8_t repeat)
{
    // 情感动作优先：立即中止正在播放的姿态，避免两套动作同时写舵机
    _player.Stop();
    _emotion.Trigger(action, repeat);
}

void MotionDirector::SetEmotionalIntensity(float pleasure, float arousal)
{
    _emotion.SetEmotionalIntensity(pleasure, arousal);
}
