// face_model.h 表情动画的纯逻辑层
//
// 不依赖 Arduino，可在 env:native 下直接测试。核心思路是把“表情”从
// 一张静态位图变成一组连续参数：EmojiId 只决定参数目标（锚点），PAD
// 情感状态在锚点上做幅度调制，眨眼、视线、呼吸、说话、特效则是叠加在
// 参数上的时间层。绘制交给 FacePainter。
#pragma once

#include <stdint.h>

#include <deskquad/core/states.h>

// 每帧最多同时存在的特效粒子
inline constexpr uint8_t kMaxFaceParticles = 10;

// ParticleKind 是表情特效粒子的种类
enum class ParticleKind : uint8_t
{
    Sparkle, // 星星闪光，开心/好奇/生气蒸汽
    Heart,   // 爱心，喜爱
    Tear,    // 泪滴，难过
    Sweat,   // 汗滴，困惑/紧张
};

// FaceParticle 是一个特效粒子，坐标单位为屏幕像素
struct FaceParticle
{
    ParticleKind kind;
    float x;
    float y;
    float vx;
    float vy;
    float phase; // 摆动相位，绘制时叠加正弦偏移
    float life;  // 剩余寿命（秒）
    float scale; // 大小系数
};

// FaceParams 是一张脸的全部可绘制参数
//
// 除显式标注外均为归一化值；绘制层只负责把参数映射为图元。
struct FaceParams
{
    float faceX;      // 整脸水平偏移（像素）
    float faceY;      // 整脸垂直偏移（像素）
    float eyeOpen;    // 眼睛睁开程度 0..1
    float eyeRound;   // 0=圆眼，1=弯月笑眼
    float eyeCross;   // 0..1，接近 1 时画成 × 眼
    float pupilX;     // 瞳孔水平偏移 -1..1
    float pupilY;     // 瞳孔垂直偏移 -1..1
    float pupilSize;  // 瞳孔大小系数 0.6..1.4
    float browAngle;  // 眉毛倾角 -1..1，正值内端上挑（难过/好奇）
    float browLift;   // 眉毛整体抬升 0..1
    float mouthCurve; // 嘴角曲率 -1..1，正值微笑
    float mouthOpen;  // 张嘴程度 0..1
    float mouthWidth; // 嘴宽系数 0.6..1.4
    float blush;      // 腮红浓度 0..1
};

// FaceState 是某一帧交给绘制层的完整快照
struct FaceState
{
    EmojiId expression;
    FaceParams params;
    FaceParticle particles[kMaxFaceParticles];
    uint8_t particleCount;
    float timeSec; // 动画时钟，供装饰层做周期运动
};

// FaceInputs 是每帧从外部喂进来的状态
struct FaceInputs
{
    float pleasure;   // PAD 愉悦度 [-1, 1]
    float arousal;    // PAD 激活度 [0, 1]
    float dominance;  // PAD 支配度 [0, 1]
    uint16_t noteHz;  // 扬声器当前音符频率，0 表示静音
    bool interacting; // 最近是否刚与用户交互（看向镜头、瞳孔放大）
};

// FaceAnimator 推进表情动画并产出 FaceState
class FaceAnimator
{
public:
    FaceAnimator();

    // Begin 复位动画时钟与随机源
    void Begin(uint32_t nowMs);

    // SetExpression 切换目标表情；参数会平滑过渡而不是硬切
    void SetExpression(EmojiId id);

    // SetInputs 更新 PAD 情感、音符与交互状态
    void SetInputs(const FaceInputs &inputs);

    // Update 按实际经过时间推进动画
    void Update(uint32_t nowMs);

    // State 返回当前帧的可绘制快照
    const FaceState &State() const { return _state; }

    // Expression 返回当前目标表情
    EmojiId Expression() const { return _expression; }

    // BlinkCount 返回累计眨眼次数，供测试与调试使用
    uint32_t BlinkCount() const { return _blinkCount; }

    // IsFacePage 判断某个 EmojiId 是否由程序化表情绘制
    static bool IsFacePage(EmojiId id);

private:
    void updateExpression(float dt);
    void updateBlink(uint32_t nowMs);
    void updateGaze(uint32_t nowMs, float dt);
    void updateTalk(float dt);
    void updateMicro(uint32_t nowMs);
    void updateParticles(uint32_t nowMs, float dt);
    void buildState(uint32_t nowMs);
    void spawnParticle(uint32_t nowMs);

    float random01();
    uint32_t randomRange(uint32_t lo, uint32_t hi);

    uint32_t _lastMs;
    uint32_t _nowMs;
    uint32_t _rng;
    float _timeSec;

    EmojiId _expression;
    FaceParams _base; // 平滑后的表情参数（不含时间层叠加）
    FaceState _state;

    float _pleasure;
    float _arousal;
    float _dominance;
    uint16_t _noteHz;
    bool _interacting;

    // 眨眼
    bool _blinking;
    float _blinkAmount;
    uint32_t _blinkStartMs;
    uint32_t _nextBlinkMs;
    uint32_t _blinkCount;

    // 视线
    float _gazeX;
    float _gazeY;
    float _gazeTargetX;
    float _gazeTargetY;
    uint32_t _nextGazeMs;

    // 说话
    float _talkPhase;
    float _talkOpen;

    // 微表情：发呆时偶尔抬眉或歪头
    uint8_t _microKind; // 0=无，1=抬眉，2=歪头
    uint32_t _microStartMs;
    uint32_t _nextMicroMs;

    // 粒子
    FaceParticle _particles[kMaxFaceParticles];
    uint8_t _particleCount;
    uint32_t _nextParticleMs;

    // 表情切换时刻，用于入场弹跳
    uint32_t _expressionChangedMs;
};
