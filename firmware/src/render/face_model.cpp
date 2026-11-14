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

    // anchorFor 是每个离散表情的眼睛参数锚点
    //
    // 字段顺序：faceX faceY eyeW eyeH eyeTilt lidSlant lidSquint
    //          eyeAsym eyeWink pupilX pupilY pupilCross pupilSize
    //          eyeHeart eyeCross talk
    //
    // 开心的笑眼靠下眼睑上抬，生气靠内低外高的上眼睑，难过靠外低内高的
    // 上眼睑与向下的瞳孔；好奇时左右眼不对称；困惑用大小眼 + 对眼；
    // 喜爱时两只眼睛直接变成会跳动的心形。
    FaceParams anchorFor(EmojiId id)
    {
        switch (id)
        {
        case EmojiId::Angry:
            return {0, 0, 14.5f, 10.5f, -0.06f, 0.90f, 0.05f, 0.00f, 0.00f, 0.15f, 0.25f, 0.00f, 0.85f, 0.0f, 0.0f, 0.0f};
        case EmojiId::Error:
            return {0, 0, 14.5f, 11.0f, 0.20f, 0.15f, 0.25f, 0.50f, 0.25f, 0.00f, 0.00f, 0.70f, 1.00f, 0.0f, 0.0f, 0.0f};
        case EmojiId::Curious:
            return {0, 0, 15.0f, 14.5f, 0.08f, 0.25f, 0.00f, 0.80f, 0.00f, 0.55f, -0.35f, 0.00f, 1.10f, 0.0f, 0.0f, 0.0f};
        case EmojiId::Love:
            return {0, 0, 14.5f, 13.0f, 0.05f, 0.00f, 0.15f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 1.15f, 1.0f, 0.0f, 0.0f};
        case EmojiId::Sad:
            return {0, 0, 14.0f, 9.0f, 0.10f, -0.70f, 0.00f, 0.00f, 0.00f, -0.10f, 0.55f, 0.00f, 0.90f, 0.0f, 0.0f, 0.0f};
        case EmojiId::Dizzy:
            return {0, 0, 14.0f, 12.0f, -0.25f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 1.00f, 0.0f, 1.0f, 0.0f};
        case EmojiId::Happy:
        default:
            return {0, 0, 15.0f, 14.0f, 0.00f, 0.00f, 0.45f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 1.00f, 0.0f, 0.0f, 0.0f};
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
    _lastMs  = nowMs;
    _nowMs   = nowMs;
    _rng     = (nowMs ^ 0x9E3779B9u) | 1u;
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

    _blinking     = false;
    _blinkAmount  = 0.0f;
    _blinkStartMs = nowMs;
    _nextBlinkMs  = nowMs + randomRange(1800, 4200);
    _blinkCount   = 0;

    _gazeX       = 0.0f;
    _gazeY       = 0.0f;
    _gazeTargetX = 0.0f;
    _gazeTargetY = 0.0f;
    _nextGazeMs  = nowMs + randomRange(800, 2000);

    _talkPhase  = 0.0f;
    _talkAmount = 0.0f;

    _microKind    = 0;
    _microDir     = 1;
    _microStartMs = nowMs;
    _nextMicroMs  = nowMs + randomRange(4000, 9000);

    _expressionChangedMs = nowMs;
}

void FaceAnimator::SetExpression(EmojiId id)
{
    if (id == _expression)
        return;

    _expression          = id;
    _state.expression    = id;
    _expressionChangedMs = _nowMs;

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
    buildState(nowMs);
}

void FaceAnimator::updateExpression(float dt)
{
    FaceParams target = anchorFor(_expression);

    // PAD 在锚点上做连续调制：愉悦让眼睛更眯、激活让眼睛更大瞳孔更亮、
    // 支配感让眼睛更宽。离散表情决定“像什么”，PAD 决定“有多强烈”。
    target.eyeH      = clampf(target.eyeH + (_arousal - 0.40f) * 2.5f, 2.0f, 16.0f);
    target.eyeW      = clampf(target.eyeW + (_dominance - 0.50f) * 1.5f, 6.0f, 17.0f);
    target.lidSquint = clampf(target.lidSquint + fmaxf(_pleasure, 0.0f) * 0.18f, 0.0f, 1.0f);
    target.eyeTilt   = clampf(target.eyeTilt + _pleasure * 0.05f, -1.0f, 1.0f);
    target.pupilSize = clampf(target.pupilSize + _arousal * 0.20f - 0.05f, 0.4f, 1.5f);

    _base.faceX      = approach(_base.faceX, target.faceX, dt, kExpressionTau);
    _base.faceY      = approach(_base.faceY, target.faceY, dt, kExpressionTau);
    _base.eyeW       = approach(_base.eyeW, target.eyeW, dt, kExpressionTau);
    _base.eyeH       = approach(_base.eyeH, target.eyeH, dt, kExpressionTau);
    _base.eyeTilt    = approach(_base.eyeTilt, target.eyeTilt, dt, kExpressionTau);
    _base.lidSlant   = approach(_base.lidSlant, target.lidSlant, dt, kExpressionTau);
    _base.lidSquint  = approach(_base.lidSquint, target.lidSquint, dt, kExpressionTau);
    _base.eyeAsym    = approach(_base.eyeAsym, target.eyeAsym, dt, kExpressionTau);
    _base.eyeWink    = approach(_base.eyeWink, target.eyeWink, dt, kExpressionTau);
    _base.pupilX     = approach(_base.pupilX, target.pupilX, dt, kExpressionTau);
    _base.pupilY     = approach(_base.pupilY, target.pupilY, dt, kExpressionTau);
    _base.pupilCross = approach(_base.pupilCross, target.pupilCross, dt, kExpressionTau);
    _base.pupilSize  = approach(_base.pupilSize, target.pupilSize, dt, kExpressionTau);
    _base.eyeHeart   = approach(_base.eyeHeart, target.eyeHeart, dt, kExpressionTau);
    _base.eyeCross   = approach(_base.eyeCross, target.eyeCross, dt, kExpressionTau);
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
            // 发呆时视线在眼眶里游移；好奇时幅度更大
            const float range = _expression == EmojiId::Curious ? 0.90f : 0.65f;
            _gazeTargetX      = (random01() * 2.0f - 1.0f) * range;
            _gazeTargetY      = (random01() * 2.0f - 1.0f) * 0.40f;
            _nextGazeMs       = nowMs + randomRange(700, 1900);
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
        // 音高越高，眼睛弹跳越快。说话时直接取振荡值，不经过平滑
        const float rate = 4.0f + static_cast<float>(_noteHz) / 260.0f;
        _talkPhase += dt * rate;
        _talkPhase -= floorf(_talkPhase);
        _talkAmount = 0.45f + 0.55f * (0.5f + 0.5f * sinf(_talkPhase * 2.0f * kPi));
    }
    else
    {
        _talkAmount = approach(_talkAmount, 0.0f, dt, 0.12f);
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
    if (_expression != EmojiId::Happy && _expression != EmojiId::Curious)
    {
        _nextMicroMs = nowMs + randomRange(4000, 8000);
        return;
    }

    _microKind    = random01() < 0.62f ? 1 : 2;
    _microDir     = random01() < 0.5f ? -1 : 1;
    _microStartMs = nowMs;
}

void FaceAnimator::buildState(uint32_t nowMs)
{
    FaceParams params = _base;

    // 呼吸：整脸与眼睛一起轻微起伏，是最廉价也最有效的“活物感”
    params.faceY += 0.8f * sinf(_timeSec * 2.0f * kPi * 0.42f);
    params.faceX += 0.35f * sinf(_timeSec * 2.0f * kPi * 0.17f);
    params.eyeH += 0.30f * sinf(_timeSec * 2.0f * kPi * 0.33f);

    // 入场弹跳：换表情后 0.34s 内轻轻一跳
    const float sinceChange = static_cast<float>(nowMs - _expressionChangedMs) / 1000.0f;
    if (sinceChange >= 0.0f && sinceChange < 0.34f)
    {
        const float u = sinceChange / 0.34f;
        params.faceY -= 4.0f * sinf(kPi * u) * (1.0f - 0.5f * u);
    }

    // 微表情：0.38s 的正弦包络，单眼眯一下或整双眼睛歪一下
    if (_microKind != 0)
    {
        float u = static_cast<float>(nowMs - _microStartMs) / 380.0f;
        if (u > 1.0f)
            u = 1.0f;
        const float envelope = sinf(kPi * u);
        if (_microKind == 1)
            params.eyeWink += static_cast<float>(_microDir) * envelope;
        else
            params.eyeTilt += static_cast<float>(_microDir) * 0.55f * envelope;
    }

    // 说话：眼睛跟着音符轻微弹跳，代替传统的一张一合的嘴
    params.talk = _talkAmount;
    params.eyeH += 1.6f * _talkAmount;
    params.faceY -= 0.6f * _talkAmount;

    // 眨眼直接乘在眼睛高度上，不参与平滑
    params.eyeH *= (1.0f - _blinkAmount);

    // 情绪抖动
    switch (_expression)
    {
    case EmojiId::Angry:
        params.faceX += 0.5f * _arousal * sinf(_timeSec * 36.0f);
        break;
    case EmojiId::Dizzy:
        params.faceX += 1.2f * sinf(_timeSec * 5.0f);
        params.faceY += 0.8f * sinf(_timeSec * 8.0f);
        break;
    case EmojiId::Error:
        params.faceX += 0.5f * sinf(_timeSec * 19.0f);
        break;
    default:
        break;
    }

    // 视线与交互：表情锚点给出视线偏置（难过向下、好奇向上），
    // 游移的注视目标叠加在偏置上。
    float gazeX = _gazeX;
    float gazeY = _gazeY;
    // 没有注视目标时叠一层缓慢的自主漂移，瞳孔始终有细微动作；
    // 与用户交互时收住漂移，保持对焦。
    if (!_interacting)
    {
        gazeX += 0.08f * sinf(_timeSec * 0.9f);
        gazeY += 0.06f * sinf(_timeSec * 0.7f + 1.7f);
    }
    params.pupilX = clampf(_base.pupilX + gazeX, -1.0f, 1.0f);
    params.pupilY = clampf(_base.pupilY + gazeY, -1.0f, 1.0f);
    if (_interacting)
        params.pupilSize += 0.12f;

    // 整双眼睛也轻微跟着视线移动，比只动瞳孔更像在“看”
    params.faceX += params.pupilX * 1.6f;
    params.faceY += params.pupilY * 0.9f;

    params.faceX     = clampf(params.faceX, -8.0f, 8.0f);
    params.faceY     = clampf(params.faceY, -8.0f, 8.0f);
    params.eyeW      = clampf(params.eyeW, 6.0f, 17.0f);
    params.eyeH      = clampf(params.eyeH, 0.0f, 16.0f);
    params.eyeTilt   = clampf(params.eyeTilt, -1.0f, 1.0f);
    params.lidSlant  = clampf(params.lidSlant, -1.0f, 1.0f);
    params.lidSquint = clampf(params.lidSquint, 0.0f, 1.0f);
    params.eyeAsym   = clampf(params.eyeAsym, -1.0f, 1.0f);
    params.eyeWink   = clampf(params.eyeWink, -1.0f, 1.0f);
    params.pupilCross = clampf(params.pupilCross, -1.0f, 1.0f);
    params.pupilSize  = clampf(params.pupilSize, 0.4f, 1.5f);
    params.eyeHeart   = clampf(params.eyeHeart, 0.0f, 1.0f);
    params.eyeCross  = clampf(params.eyeCross, 0.0f, 1.0f);
    params.talk      = clampf(params.talk, 0.0f, 1.0f);

    _state.expression = _expression;
    _state.params     = params;
    _state.timeSec    = _timeSec;
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
