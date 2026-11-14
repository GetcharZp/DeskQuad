// face_model.h 表情动画的纯逻辑层
//
// 不依赖 Arduino，可在 env:native 下直接测试。整套表情只有两只眼睛：
// EmojiId 决定眼睛的目标形状（大小、倾角、眯眼、左右差异），PAD 情感
// 状态做连续调制，眨眼、视线、呼吸、说话、微表情则是叠加在参数上的
// 时间层。绘制交给 FacePainter。
#pragma once

#include <stdint.h>

#include <deskquad/core/states.h>

// FaceParams 是一张脸的全部可绘制参数
//
// 除显式标注外均为归一化值；绘制层只负责把参数映射为图元。
struct FaceParams
{
    float faceX;      // 双眼整体水平偏移（像素）
    float faceY;      // 双眼整体垂直偏移（像素）
    float eyeW;       // 单眼半宽（像素）6..17
    float eyeH;       // 单眼半高（像素）0..16
    float eyeTilt;    // 整眼旋转 -1..1，映射为约 ±14°
    float lidSlant;   // 上眼睑倾角 -1..1：正值内端下压（生气），负值外端下垂（难过）
    float lidSquint;  // 下眼睑上抬 0..1，越大越像笑眼
    float eyeAsym;    // 左右眼差异 -1..1：正值左眼更大更高
    float eyeWink;    // 单眼额外眯起 -1..1：负值眯左眼，正值眯右眼
    float pupilX;     // 瞳孔水平偏移 -1..1
    float pupilY;     // 瞳孔垂直偏移 -1..1
    float pupilCross; // 双眼对视偏移 0..1：左眼向右、右眼向左，越大越像“对眼”
    float pupilSize;  // 瞳孔大小系数 0.4..1.5
    float eyeHeart;   // 心形眼程度 0..1
    float eyeCross;   // 眩晕螺旋眼程度 0..1
    float talk;       // 说话能量 0..1，驱动眼睛弹跳
};

// FaceState 是某一帧交给绘制层的完整快照
struct FaceState
{
    EmojiId expression;
    FaceParams params;
    float timeSec; // 动画时钟，驱动眩晕螺旋眼
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
    void buildState(uint32_t nowMs);

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

    // 说话：音符能量驱动眼睛弹跳
    float _talkPhase;
    float _talkAmount;

    // 微表情：发呆时偶尔单眼眯一下或歪头
    uint8_t _microKind; // 0=无，1=单眼眯，2=歪头
    int8_t _microDir;   // 眯哪只眼 / 往哪边歪
    uint32_t _microStartMs;
    uint32_t _nextMicroMs;

    // 表情切换时刻，用于入场弹跳
    uint32_t _expressionChangedMs;
};
