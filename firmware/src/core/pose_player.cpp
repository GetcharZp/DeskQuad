#include <deskquad/core/pose_player.h>

#include <deskquad/core/clock.h>

PosePlayer::PosePlayer(IServoWriter &writer)
    : _writer(writer), _program(nullptr), _stepIndex(0), _loopLeft(1), _stepStartMs(0), _playing(false)
{
}

void PosePlayer::Play(const PoseProgram &program)
{
    _program     = &program;
    _stepIndex   = 0;
    _loopLeft    = program.loopCount == 0 ? 1 : program.loopCount;
    _stepStartMs = NowMs();
    _playing     = program.stepCount > 0;
    if (_playing)
        applyStep(program.steps[0]);
}

void PosePlayer::Stop()
{
    _playing = false;
    _program = nullptr;
}

void PosePlayer::Update()
{
    if (!_playing || _program == nullptr)
        return;
    if (NowMs() - _stepStartMs < _program->steps[_stepIndex].holdMs)
        return;

    const bool insideLoop = _stepIndex + 1 < _program->loopSteps;
    const bool atLoopEnd  = _stepIndex + 1 == _program->loopSteps;

    if (insideLoop)
    {
        _stepIndex++;
    }
    else if (atLoopEnd && _loopLeft > 1)
    {
        _loopLeft--;
        _stepIndex = 0;
    }
    else if (_stepIndex + 1 < _program->stepCount)
    {
        _stepIndex++;
    }
    else
    {
        _playing = false;
        return;
    }

    _stepStartMs = NowMs();
    applyStep(_program->steps[_stepIndex]);
}

void PosePlayer::applyStep(const PoseStep &step)
{
    for (uint8_t channel = 0; channel < 4; channel++)
    {
        const int16_t angle = step.servo[channel];
        if (angle == kHoldAngle)
            continue;
        _writer.Write(channel, angle);
    }
}
