// gesture_recognizer.h 用背景差分和轮廓几何识别桌面手势
//
// 输入是 80×60 灰度帧。每帧依次做背景差分、二值化、3×3 多数滤波和
// 最大连通域提取，再用包围盒、填充率、顶部指尖数量和质心时序判定类别。
#pragma once

#include <stdint.h>

namespace deskquad
{

    // 手势识别输入尺寸，也是摄像头灰度下采样的目标尺寸
    inline constexpr int kGestureFrameWidth  = 80;
    inline constexpr int kGestureFrameHeight = 60;
    inline constexpr int kGesturePixelCount  = kGestureFrameWidth * kGestureFrameHeight;

} // namespace deskquad

// 手势分类
enum GestureClass : uint8_t
{
    GESTURE_NONE  = 0, // 无手势
    GESTURE_WAVE  = 1, // 挥手
    GESTURE_PALM  = 2, // 伸掌/停止
    GESTURE_FIST  = 3, // 握拳
    GESTURE_POINT = 4, // 指点
};

// GestureResult 是一次识别的结果
struct GestureResult
{
    GestureClass gesture = GESTURE_NONE; // 分类结果
    float confidence     = 0.0f;         // 置信度 [0, 1]

    // 以下为串口调参用的中间量
    uint16_t blobArea     = 0; // 最大前景连通域面积
    uint8_t bboxWidth     = 0; // 前景包围盒宽度
    uint8_t bboxHeight    = 0; // 前景包围盒高度
    uint8_t fingerCount   = 0; // 顶部轮廓检测到的指尖数量
    uint8_t waveReversals = 0; // 时序窗口内的水平反向次数
};

// GestureRecognizer 识别静态手势与挥手
class GestureRecognizer
{
public:
    GestureRecognizer();

    // Classify 处理一帧灰度图并返回识别结果
    //
    // # Params:
    //
    //	grayBuffer: 长度 kGesturePixelCount 的灰度缓冲，行优先
    //
    // # Returns:
    //
    //	识别结果。首次调用只学习背景，始终返回 GESTURE_NONE；
    //	grayBuffer 为空时同样返回 GESTURE_NONE。
    GestureResult Classify(const uint8_t *grayBuffer);

    // Reset 丢弃背景模型和时序历史，下一帧重新学习背景
    void Reset();

    // IsBackgroundReady 返回背景模型是否已经建立
    bool IsBackgroundReady() const { return _backgroundReady; }

private:
    // BlobStats 描述当前最大前景连通域
    struct BlobStats
    {
        uint16_t area = 0; // 像素数
        uint8_t minX  = 0; // 包围盒左边界
        uint8_t maxX  = 0; // 包围盒右边界
        uint8_t minY  = 0; // 包围盒上边界
        uint8_t maxY  = 0; // 包围盒下边界
        uint32_t sumX = 0; // 质心累计，除以 area 得到质心
        uint32_t sumY = 0; // 质心累计，除以 area 得到质心
    };

    // 分类阈值，实机调参时优先动这里的值
    static constexpr int kDiffThreshold     = 26;   // 前景灰度差阈值
    static constexpr int kBackgroundShift   = 5;    // 背景 EMA 移位，1/32
    static constexpr int kMinBlobArea       = 80;   // 小于此值视为噪声
    static constexpr int kMaxBlobArea       = 3200; // 大于此值视为遮挡
    static constexpr int kMaxForegroundRate = 65;   // 前景占比超过此值按场景切换处理
    static constexpr int kWaveWindow        = 6;    // 质心时序窗口
    static constexpr int kWaveMinReversals  = 1;    // 至少一次水平反向（一次来回）
    static constexpr int kWaveMinAmplitude  = 6;    // 反向幅度至少 6 像素

    void updateBackground(const uint8_t *grayBuffer);
    void buildMask(const uint8_t *grayBuffer, int offset, int *foregroundCount);
    void filterMask();
    BlobStats findLargestBlob();
    void pushHistory(uint8_t centroidX);
    void clearHistory();
    bool detectWave(uint8_t *reversalsOut);

    static uint8_t countFingertips(const uint8_t *topY, const BlobStats &stats);
    static uint8_t countTopColumns(const uint8_t *topY, const BlobStats &stats);
    static float clampConfidence(float value);

    bool _backgroundReady;
    uint8_t _background[deskquad::kGesturePixelCount];
    uint8_t _rawMask[deskquad::kGesturePixelCount];
    uint8_t _mask[deskquad::kGesturePixelCount];
    uint16_t _queue[deskquad::kGesturePixelCount];
    uint8_t _topY[deskquad::kGestureFrameWidth];
    uint8_t _bestTopY[deskquad::kGestureFrameWidth];

    // 质心水平时序，用于挥手检测
    uint8_t _historyX[kWaveWindow];
    uint8_t _historyCount;
};
