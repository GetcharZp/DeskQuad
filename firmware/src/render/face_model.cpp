#include <deskquad/render/face_model.h>

#include <math.h>
#include <string.h>

namespace
{
    constexpr float kPi = 3.14159265f;

    // 表情参数平滑的时间常数（秒）：越小切换越快
    constexpr float kExpressionTau = 0.10f;
    constexpr float kGazeTau       = 0.045f;

    // 一次眨眼的三个阶段的累计时刻（秒）
    constexpr float kBlinkCloseEnd = 0.055f;
    constexpr float kBlinkHoldEnd  = 0.075f;
    constexpr float kBlinkOpenEnd  = 0.160f;

    float clampf(float value, float lo, float hi)
    {
        if (value < lo)
            return lo;
        if (value > hi)
            return hi;
        return value;
    }

    // approach 是帧率无关的指数趋近
    float approach(float current, float target, float dt, float tau)
    {
        if (dt <= 0.0f)
            return current;
        return current + (target - current) * (1.0f - expf(-dt / tau));
    }

    bool timeReached(uint32_t now, uint32_t deadline)
    {
        return static_cast<int32_t>(now - deadline) >= 0;
    }

    // anchorFor 是每个离散表情的参数锚点
    //
    // 字段顺序：faceX faceY eyeOpen eyeRound eyeCross pupilX pupilY pupilSize
    //          browAngle browLift mouthCurve mouthOpen mouthWidth blush
    FaceParams anchorFor(EmojiId id)
    {
        switch (id)
        {
        case EmojiId::Angry:
            return {0, 0, 0.62f, 0.00f, 0.0f, 0.15f, 0.10f, 0.90f, -0.80f, 0.05f, -0.80f, 0.05f, 0.85f, 0.00f};
        case EmojiId::Error:
            return {0, 0, 0.80f, 0.10f, 0.0f, 0.35f, 0.25f, 1.00f, 0.45f, 0.55f, -0.30f, 0.25f, 0.70f, 0.00f};
        case EmojiId::Curious:
            return {1, 0, 1.00f, 0.00f, 0.0f, 0.50f, -0.20f, 1.15f, 0.35f, 0.85f, 0.15f, 0.35f, 0.55f, 0.05f};
        case EmojiId::Love:
            return {0, 0, 0.85f, 1.00f, 0.0f, 0.00f, 0.10f, 1.10f, 0.25f, 0.55f, 0.95f, 0.10f, 0.95f, 1.00f};
        case EmojiId::Sad:
            return {0, 0, 0.50f, 0.25f, 0.0f, -0.10f, 0.35f, 0.95f, 0.60f, 0.15f, -0.90f, 0.00f, 0.90f, 0.00f};
        case EmojiId::Dizzy:
            return {0, 0, 0.75f, 0.00f, 1.0f, 0.00f, 0.00f, 1.00f, -0.15f, 0.10f, -0.15f, 0.45f, 0.80f, 0.00f};
        case EmojiId::Happy:
        default:
            return {0, 0, 0.80f, 0.85f, 0.0f, 0.00f, 0.00f, 1.00f, 0.05f, 0.45f, 0.85f, 0.12f, 1.00f, 0.25f};
        }
    }

} // namespace

FaceAnimator::FaceAnimator()
{
    Begin(0);
}

bool FaceAnimator::IsFacePage(EmojiId id)
{
    return static_cast<uint8_t>(id) <= static_cast<uint8_t>(EmojiId::Dizzy);
}

void FaceAnimator::Begin(uint32_t nowMs)
{
    _lastMs = nowMs;
    _nowMs  = nowMs;
    _rng    = (nowMs ^ 0x9E3779B9u) | 1u;
    _timeSec = 0.0f;

    _expression = EmojiId::Happy;
    _base       = anchorFor(_expression);
    memset(&_state, 0, sizeof(_state));
    _state.expression = _expression;
    _state.params     = _base;

    _pleasure    = 0.0f;
    _arousal    = 0.30f;
    _dominance   = 0.50f;
    _noteHz      = 0;
    _interacting = false;

    _blinking       = false;
    _blinkAmount    = 0.0f;
    _blinkStartMs   = nowMs;
    _nextBlinkMs    = nowMs + randomRange(1800, 4200);
    _blinkCount     = 0;

    _gazeX       = 0.0f;
    _gazeY       = 0.0f;
    _gazeTargetX = 0.0f;
    _gazeTargetY = 0.0f;
    _nextGazeMs  = nowMs + randomRange(800, 2000);

    _talkPhase    = 0.0f;
    _talkOpen     = 0.0f;
    _microKind    = 0;
    _microStartMs = nowMs;
    _nextMicroMs  = nowMs + randomRange(4000, 9000);

    _particleCount       = 0;
    _nextParticleMs      = nowMs + 400;
    _expressionChangedMs = nowMs;
}

void FaceAnimator::SetExpression(EmojiId id)
{
    if (id == _expression)
        return;

    _expression          = id;
    _state.expression    = id;
    _expressionChangedMs = _nowMs;
    _nextParticleMs      = _nowMs + 250;

    // 打断进行中的微表情，避免换脸后还挂着上一个表情的小动作
    _microKind   = 0;
    _nextMicroMs = _nowMs + randomRange(4000, 9000);

    // 换表情时轻微打断眨眼节奏，避免刚切换就闭眼
    if (_blinking)
    {
        _blinking    = false;
        _blinkAmount = 0.0f;
        _nextBlinkMs = _nowMs + randomRange(1600, 3200);
    }
}

void FaceAnimator::SetInputs(const FaceInputs &inputs)
{
    _pleasure    = clampf(inputs.pleasure, -1.0f, 1.0f);
    _arousal     = clampf(inputs.arousal, 0.0f, 1.0f);
    _dominance   = clampf(inputs.dominance, 0.0f, 1.0f);
    _noteHz      = inputs.noteHz;
    _interacting = inputs.interacting;
}

void FaceAnimator::Update(uint32_t nowMs)
{
    float dt = static_cast<float>(nowMs - _lastMs) / 1000.0f;
    if (dt > 0.25f)
        dt = 0.25f; // 长时间停顿后不跳变
    _lastMs = nowMs;
    _nowMs  = nowMs;
    _timeSec += dt;

    updateExpression(dt);
    updateBlink(nowMs);
    updateGaze(nowMs, dt);
    updateTalk(dt);
    updateMicro(nowMs);
    updateParticles(nowMs, dt);
    buildState(nowMs);
}

void FaceAnimator::updateExpression(float dt)
{
    FaceParams target = anchorFor(_expression);

    // PAD 在锚点上做连续调制：愉悦改嘴角与腮红，激活改睁眼与瞳孔，
    // 支配感改眉毛高度。离散表情决定“像什么”，PAD 决定“有多强烈”。
    target.mouthCurve = clampf(target.mouthCurve + _pleasure * 0.20f, -1.0f, 1.0f);
    target.eyeOpen    = clampf(target.eyeOpen + (_arousal - 0.40f) * 0.25f, 0.15f, 1.0f);
    target.pupilSize  = clampf(target.pupilSize + _arousal * 0.20f - 0.05f, 0.6f, 1.4f);
    target.browLift   = clampf(target.browLift + _dominance * 0.15f, 0.0f, 1.0f);
    target.blush      = clampf(target.blush + fmaxf(_pleasure, 0.0f) * 0.20f, 0.0f, 1.0f);

    _base.faceX      = approach(_base.faceX, target.faceX, dt, kExpressionTau);
    _base.faceY      = approach(_base.faceY, target.faceY, dt, kExpressionTau);
    _base.eyeOpen    = approach(_base.eyeOpen, target.eyeOpen, dt, kExpressionTau);
    _base.eyeRound   = approach(_base.eyeRound, target.eyeRound, dt, kExpressionTau);
    _base.eyeCross   = approach(_base.eyeCross, target.eyeCross, dt, kExpressionTau);
    _base.pupilSize  = approach(_base.pupilSize, target.pupilSize, dt, kExpressionTau);
    _base.browAngle  = approach(_base.browAngle, target.browAngle, dt, kExpressionTau);
    _base.browLift   = approach(_base.browLift, target.browLift, dt, kExpressionTau);
    _base.mouthCurve = approach(_base.mouthCurve, target.mouthCurve, dt, kExpressionTau);
    _base.mouthOpen  = approach(_base.mouthOpen, target.mouthOpen, dt, kExpressionTau);
    _base.mouthWidth = approach(_base.mouthWidth, target.mouthWidth, dt, kExpressionTau);
    _base.blush      = approach(_base.blush, target.blush, dt, kExpressionTau);
}

void FaceAnimator::updateBlink(uint32_t nowMs)
{
    if (_blinking)
    {
        const float elapsed = static_cast<float>(nowMs - _blinkStartMs) / 1000.0f;
        if (elapsed < kBlinkCloseEnd)
        {
            _blinkAmount = elapsed / kBlinkCloseEnd;
        }
        else if (elapsed < kBlinkHoldEnd)
        {
            _blinkAmount = 1.0f;
        }
        else if (elapsed < kBlinkOpenEnd)
        {
            _blinkAmount = 1.0f - (elapsed - kBlinkHoldEnd) / (kBlinkOpenEnd - kBlinkHoldEnd);
        }
        else
        {
            _blinking    = false;
            _blinkAmount = 0.0f;

            // 一定概率连眨两下，看起来更像活物
            if (random01() < 0.16f)
                _nextBlinkMs = nowMs + 140;
            else
                _nextBlinkMs = nowMs + randomRange(2200, 6500);
        }
        return;
    }

    if (timeReached(nowMs, _nextBlinkMs))
    {
        _blinking     = true;
        _blinkStartMs = nowMs;
        _blinkAmount  = 0.0f;
        _blinkCount++;
    }
}

void FaceAnimator::updateGaze(uint32_t nowMs, float dt)
{
    if (timeReached(nowMs, _nextGazeMs))
    {
        if (_interacting)
        {
            // 用户刚有动作：盯住正前方
            _gazeTargetX = 0.0f;
            _gazeTargetY = 0.0f;
            _nextGazeMs  = nowMs + randomRange(700, 1400);
        }
        else
        {
            // 发呆时视线小范围游移；好奇时幅度更大
            const float range = _expression == EmojiId::Curious ? 0.80f : 0.45f;
            _gazeTargetX      = (random01() * 2.0f - 1.0f) * range;
            _gazeTargetY      = (random01() * 2.0f - 1.0f) * 0.35f;
            _nextGazeMs       = nowMs + randomRange(900, 2600);
        }
    }

    if (_expression == EmojiId::Dizzy)
    {
        // 眩晕时视线画圈
        _gazeX = 0.85f * sinf(_timeSec * 7.0f);
        _gazeY = 0.55f * cosf(_timeSec * 9.0f);
        return;
    }

    const float tau = _interacting ? 0.035f : kGazeTau;
    _gazeX          = approach(_gazeX, _gazeTargetX, dt, tau);
    _gazeY          = approach(_gazeY, _gazeTargetY, dt, tau);
}

void FaceAnimator::updateTalk(float dt)
{
    if (_noteHz > 0)
    {
        // 音高越高，嘴开合越快；说话时嘴型直接取振荡值，不经过平滑，
        // 否则 8~15Hz 的开合会被指数平滑吃掉。
        const float rate = 5.5f + static_cast<float>(_noteHz) / 180.0f;
        _talkPhase += dt * rate;
        _talkPhase -= floorf(_talkPhase);
        _talkOpen = 0.35f + 0.45f * fabsf(sinf(_talkPhase * 2.0f * kPi));
    }
    else
    {
        _talkOpen = approach(_talkOpen, 0.0f, dt, 0.08f);
    }
}

void FaceAnimator::updateMicro(uint32_t nowMs)
{
    if (_microKind != 0)
    {
        if (nowMs - _microStartMs >= 380)
        {
            _microKind   = 0;
            _nextMicroMs = nowMs + randomRange(6000, 13000);
        }
        return;
    }

    if (!timeReached(nowMs, _nextMicroMs))
        return;

    // 只在轻松的表情上做小动作：生气和眩晕时不卖萌
    if (_expression != EmojiId::Happy && _expression != EmojiId::Curious && _expression != EmojiId::Love)
    {
        _nextMicroMs = nowMs + randomRange(4000, 8000);
        return;
    }

    _microKind    = random01() < 0.6f ? 1 : 2;
    _microStartMs = nowMs;
}

void FaceAnimator::spawnParticle(uint32_t nowMs)
{
    if (_particleCount >= kMaxFaceParticles)
    {
        _nextParticleMs = nowMs + 500;
        return;
    }

    FaceParticle particle = {};
    particle.scale        = 1.0f;

    switch (_expression)
    {
    case EmojiId::Love:
        particle.kind  = ParticleKind::Heart;
        particle.x     = 34.0f + random01() * 60.0f;
        particle.y     = 44.0f;
        particle.vx    = (random01() * 2.0f - 1.0f) * 8.0f;
        particle.vy    = -14.0f - random01() * 6.0f;
        particle.life  = 1.8f + random01() * 0.6f;
        particle.scale = 0.9f + random01() * 0.3f;
        _nextParticleMs = nowMs + randomRange(380, 650);
        break;
    case EmojiId::Sad:
        particle.kind   = ParticleKind::Tear;
        particle.x      = ((_rng & 1u) != 0u) ? 42.0f : 86.0f;
        particle.y      = 34.0f;
        particle.vx     = 0.5f;
        particle.vy     = 6.0f;
        particle.life   = 1.0f;
        _nextParticleMs = nowMs + randomRange(1200, 2100);
        break;
    case EmojiId::Angry:
        particle.kind   = ParticleKind::Sparkle;
        particle.x      = 52.0f + random01() * 24.0f;
        particle.y      = 12.0f;
        particle.vx     = (random01() * 2.0f - 1.0f) * 6.0f;
        particle.vy     = -12.0f;
        particle.life   = 0.7f;
        particle.scale  = 0.7f;
        _nextParticleMs = nowMs + randomRange(700, 1100);
        break;
    case EmojiId::Curious:
        particle.kind   = ParticleKind::Sparkle;
        particle.x      = 96.0f + random01() * 16.0f;
        particle.y      = 8.0f + random01() * 8.0f;
        particle.vx     = 0.0f;
        particle.vy     = -3.0f;
        particle.life   = 1.0f;
        particle.scale  = 1.0f;
        _nextParticleMs = nowMs + randomRange(1000, 1700);
        break;
    case EmojiId::Error:
        particle.kind   = ParticleKind::Sweat;
        particle.x      = 100.0f + random01() * 8.0f;
        particle.y      = 10.0f;
        particle.vx     = 0.5f;
        particle.vy     = 9.0f;
        particle.life   = 1.4f;
        _nextParticleMs = nowMs + randomRange(1500, 2400);
        break;
    case EmojiId::Happy:
        particle.kind   = ParticleKind::Sparkle;
        particle.x      = 10.0f + random01() * 108.0f;
        particle.y      = 6.0f + random01() * 12.0f;
        particle.vx     = 0.0f;
        particle.vy     = -5.0f;
        particle.life   = 0.9f;
        particle.scale  = 0.9f;
        _nextParticleMs = nowMs + randomRange(1100, 1900);
        break;
    case EmojiId::Dizzy:
    default:
        _nextParticleMs = nowMs + 1000;
        return;
    }

    particle.phase = random01() * 2.0f * kPi;
    _particles[_particleCount++] = particle;
}

void FaceAnimator::updateParticles(uint32_t nowMs, float dt)
{
    for (uint8_t i = 0; i < _particleCount;)
    {
        FaceParticle &particle = _particles[i];

        if (particle.kind == ParticleKind::Tear || particle.kind == ParticleKind::Sweat)
            particle.vy += 24.0f * dt;

        particle.x += particle.vx * dt;
        particle.y += particle.vy * dt;
        particle.life -= dt;

        if (particle.life <= 0.0f)
        {
            _particles[i] = _particles[_particleCount - 1];
            _particleCount--;
            continue;
        }
        i++;
    }

    if (timeReached(nowMs, _nextParticleMs))
        spawnParticle(nowMs);
}

void FaceAnimator::buildState(uint32_t nowMs)
{
    FaceParams params = _base;

    // 呼吸：整脸 ±1px 的慢速起伏，是最廉价也最有效的“活物感”
    params.faceY += 0.9f * sinf(_timeSec * 2.0f * kPi * 0.42f);
    params.faceX += 0.45f * sinf(_timeSec * 2.0f * kPi * 0.17f);

    // 入场弹跳：换表情后 0.34s 内轻轻一跳
    const float sinceChange = static_cast<float>(nowMs - _expressionChangedMs) / 1000.0f;
    if (sinceChange >= 0.0f && sinceChange < 0.34f)
    {
        const float u = sinceChange / 0.34f;
        params.faceY -= 4.0f * sinf(kPi * u) * (1.0f - 0.5f * u);
    }

    // 微表情：0.38s 的正弦包络，抬眉或歪头
    if (_microKind != 0)
    {
        float u = static_cast<float>(nowMs - _microStartMs) / 380.0f;
        if (u > 1.0f)
            u = 1.0f;
        const float envelope = sinf(kPi * u);
        if (_microKind == 1)
        {
            params.browLift = fminf(1.0f, params.browLift + 0.35f * envelope);
            params.eyeOpen  = fminf(1.0f, params.eyeOpen + 0.06f * envelope);
        }
        else
        {
            params.faceX += 2.0f * envelope;
        }
    }

    // 说话嘴型
    if (_talkOpen > 0.02f)
    {
        params.mouthOpen  = fmaxf(params.mouthOpen, _talkOpen);
        params.mouthWidth = fmaxf(params.mouthWidth, 0.95f);
    }

    // 眨眼直接乘在睁眼程度上，不参与平滑
    params.eyeOpen *= (1.0f - _blinkAmount);

    // 情绪抖动
    switch (_expression)
    {
    case EmojiId::Angry:
        params.faceX += 0.8f * _arousal * sinf(_timeSec * 36.0f);
        break;
    case EmojiId::Dizzy:
        params.faceX += 1.6f * sinf(_timeSec * 5.0f);
        params.faceY += 1.0f * sinf(_timeSec * 8.0f);
        break;
    case EmojiId::Error:
        params.faceX += 0.7f * sinf(_timeSec * 19.0f);
        break;
    default:
        break;
    }

    // 视线与交互
    params.pupilX = clampf(_gazeX, -1.0f, 1.0f);
    params.pupilY = clampf(_gazeY, -1.0f, 1.0f);
    if (_interacting)
        params.pupilSize += 0.12f;

    params.faceX      = clampf(params.faceX, -7.0f, 7.0f);
    params.faceY      = clampf(params.faceY, -9.0f, 9.0f);
    params.eyeOpen    = clampf(params.eyeOpen, 0.0f, 1.0f);
    params.eyeRound   = clampf(params.eyeRound, 0.0f, 1.0f);
    params.eyeCross   = clampf(params.eyeCross, 0.0f, 1.0f);
    params.pupilSize  = clampf(params.pupilSize, 0.6f, 1.4f);
    params.browAngle  = clampf(params.browAngle, -1.0f, 1.0f);
    params.browLift   = clampf(params.browLift, 0.0f, 1.0f);
    params.mouthCurve = clampf(params.mouthCurve, -1.0f, 1.0f);
    params.mouthOpen  = clampf(params.mouthOpen, 0.0f, 1.0f);
    params.mouthWidth = clampf(params.mouthWidth, 0.6f, 1.4f);
    params.blush      = clampf(params.blush, 0.0f, 1.0f);

    _state.expression    = _expression;
    _state.params        = params;
    _state.timeSec       = _timeSec;
    _state.particleCount = _particleCount;
    for (uint8_t i = 0; i < _particleCount; i++)
        _state.particles[i] = _particles[i];
}

float FaceAnimator::random01()
{
    _rng ^= _rng << 13;
    _rng ^= _rng >> 17;
    _rng ^= _rng << 5;
    return static_cast<float>(_rng & 0xFFFFu) / 65535.0f;
}

uint32_t FaceAnimator::randomRange(uint32_t lo, uint32_t hi)
{
    if (hi <= lo)
        return lo;
    return lo + static_cast<uint32_t>(random01() * static_cast<float>(hi - lo));
}
