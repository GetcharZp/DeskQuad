// gesture_recognizer.cpp 背景差分与轮廓规则的手势识别实现
#include <deskquad/perception/gesture_recognizer.h>

#include <string.h>

namespace
{

    // kMinHistoryForWave 是挥手判定至少需要的有效帧数
    constexpr int kMinHistoryForWave = 3;

    // kTipProminence 是判定指尖所需的最小轮廓起伏，滤掉噪声
    constexpr int kTipProminence = 3;

    // kTipSearchRadius 是计算起伏时向两侧搜索的列数
    constexpr int kTipSearchRadius = 4;

} // namespace

GestureRecognizer::GestureRecognizer()
{
    Reset();
}

void GestureRecognizer::Reset()
{
    _backgroundReady = false;
    memset(_background, 0, sizeof(_background));
    memset(_rawMask, 0, sizeof(_rawMask));
    memset(_mask, 0, sizeof(_mask));
    memset(_topY, 0xFF, sizeof(_topY));
    memset(_bestTopY, 0xFF, sizeof(_bestTopY));
    clearHistory();
}

GestureResult GestureRecognizer::Classify(const uint8_t *grayBuffer)
{
    GestureResult result;
    if (grayBuffer == nullptr)
        return result;

    // 第一帧只用来建立背景
    if (!_backgroundReady)
    {
        memcpy(_background, grayBuffer, deskquad::kGesturePixelCount);
        _backgroundReady = true;
        clearHistory();
        return result;
    }

    // 先补偿整体亮度漂移，避免自动曝光把整幅画面判成前景
    int32_t sumGray       = 0;
    int32_t sumBackground = 0;
    for (int i = 0; i < deskquad::kGesturePixelCount; i++)
    {
        sumGray += grayBuffer[i];
        sumBackground += _background[i];
    }
    int offset = (int)((sumGray - sumBackground) / deskquad::kGesturePixelCount);
    if (offset > 20)
        offset = 20;
    if (offset < -20)
        offset = -20;

    int foregroundCount = 0;
    buildMask(grayBuffer, offset, &foregroundCount);

    // 前景铺满画面说明场景整体变了，直接重建背景并等待下一帧
    if (foregroundCount * 100 > deskquad::kGesturePixelCount * kMaxForegroundRate)
    {
        memcpy(_background, grayBuffer, deskquad::kGesturePixelCount);
        clearHistory();
        return result;
    }

    updateBackground(grayBuffer);
    filterMask();

    const BlobStats stats = findLargestBlob();
    if (stats.area < kMinBlobArea || stats.area > kMaxBlobArea)
    {
        clearHistory();
        return result;
    }

    result.blobArea    = stats.area;
    result.bboxWidth   = (uint8_t)(stats.maxX - stats.minX + 1);
    result.bboxHeight  = (uint8_t)(stats.maxY - stats.minY + 1);
    result.fingerCount = countFingertips(_bestTopY, stats);

    // 太小的目标无法可靠判断形状，但仍然参与挥手时序
    if (result.bboxWidth < 8 || result.bboxHeight < 8)
    {
        clearHistory();
        return result;
    }

    const uint8_t centroidX = (uint8_t)(stats.sumX / stats.area);
    pushHistory(centroidX);

    uint8_t reversals = 0;
    if (detectWave(&reversals))
    {
        result.gesture       = GESTURE_WAVE;
        result.confidence    = clampConfidence(0.60f + 0.08f * reversals);
        result.waveReversals = reversals;
        // 挥手已消费掉这段轨迹，清空历史避免连续触发
        clearHistory();
        return result;
    }
    result.waveReversals = reversals;

    const int boxArea     = (int)result.bboxWidth * (int)result.bboxHeight;
    const int fillPercent = boxArea > 0 ? (int)stats.area * 100 / boxArea : 0;
    const int topColumns  = countTopColumns(_bestTopY, stats);

    // 指点：顶部收窄成一根手指，整体明显高于宽
    const bool narrowTop  = topColumns * 100 <= (int)result.bboxWidth * 40;
    const bool tallEnough = (int)result.bboxHeight * 10 >= (int)result.bboxWidth * 11;
    if (narrowTop && result.fingerCount <= 1 && tallEnough)
    {
        const float margin = (0.40f - (float)topColumns / (float)result.bboxWidth) / 0.40f;
        result.gesture     = GESTURE_POINT;
        result.confidence  = clampConfidence(0.55f + 0.30f * margin);
        return result;
    }

    // 张开的手掌会在顶部留下多个指尖
    if (result.fingerCount >= 3)
    {
        result.gesture    = GESTURE_PALM;
        result.confidence = clampConfidence(0.55f + 0.07f * (float)(result.fingerCount - 2));
        return result;
    }

    if (result.fingerCount == 2 && fillPercent < 70)
    {
        result.gesture    = GESTURE_PALM;
        result.confidence = 0.58f;
        return result;
    }

    // 握拳：轮廓紧凑，顶部没有明显指尖
    if (fillPercent >= 58 && result.fingerCount <= 2)
    {
        result.gesture    = GESTURE_FIST;
        result.confidence = clampConfidence(0.55f + 0.005f * (float)(fillPercent - 58));
        return result;
    }

    return result;
}

void GestureRecognizer::buildMask(const uint8_t *grayBuffer, int offset, int *foregroundCount)
{
    int count = 0;
    for (int i = 0; i < deskquad::kGesturePixelCount; i++)
    {
        const int diff = (int)grayBuffer[i] - (int)_background[i] - offset;
        if (diff > kDiffThreshold || diff < -kDiffThreshold)
        {
            _rawMask[i] = 1;
            count++;
        }
        else
        {
            _rawMask[i] = 0;
        }
    }
    *foregroundCount = count;
}

void GestureRecognizer::updateBackground(const uint8_t *grayBuffer)
{
    for (int i = 0; i < deskquad::kGesturePixelCount; i++)
    {
        if (_rawMask[i])
            continue;
        const int delta = (int)grayBuffer[i] - (int)_background[i];
        _background[i]  = (uint8_t)((int)_background[i] + delta / (1 << kBackgroundShift));
    }
}

void GestureRecognizer::filterMask()
{
    const int width  = deskquad::kGestureFrameWidth;
    const int height = deskquad::kGestureFrameHeight;

    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            int neighbors = 0;
            for (int dy = -1; dy <= 1; dy++)
            {
                const int ny = y + dy;
                if (ny < 0 || ny >= height)
                    continue;
                for (int dx = -1; dx <= 1; dx++)
                {
                    const int nx = x + dx;
                    if (nx < 0 || nx >= width)
                        continue;
                    if (_rawMask[ny * width + nx])
                        neighbors++;
                }
            }
            _mask[y * width + x] = neighbors >= 5 ? 1 : 0;
        }
    }
}

GestureRecognizer::BlobStats GestureRecognizer::findLargestBlob()
{
    const int width  = deskquad::kGestureFrameWidth;
    const int height = deskquad::kGestureFrameHeight;
    BlobStats best;

    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            const int start = y * width + x;
            if (!_mask[start])
                continue;

            // 每个连通域单独记录顶部轮廓，最大者胜出后整体保留
            memset(_topY, 0xFF, sizeof(_topY));

            BlobStats current;
            current.minX = (uint8_t)x;
            current.maxX = (uint8_t)x;
            current.minY = (uint8_t)y;
            current.maxY = (uint8_t)y;

            int head       = 0;
            int tail       = 0;
            _queue[tail++] = (uint16_t)start;
            _mask[start]   = 0;

            while (head < tail)
            {
                const int index = _queue[head++];
                const int px    = index % width;
                const int py    = index / width;

                current.area++;
                current.sumX += (uint32_t)px;
                current.sumY += (uint32_t)py;
                if (px < current.minX)
                    current.minX = (uint8_t)px;
                if (px > current.maxX)
                    current.maxX = (uint8_t)px;
                if (py < current.minY)
                    current.minY = (uint8_t)py;
                if (py > current.maxY)
                    current.maxY = (uint8_t)py;
                if (py < (int)_topY[px])
                    _topY[px] = (uint8_t)py;

                for (int dy = -1; dy <= 1; dy++)
                {
                    const int ny = py + dy;
                    if (ny < 0 || ny >= height)
                        continue;
                    for (int dx = -1; dx <= 1; dx++)
                    {
                        if (dx == 0 && dy == 0)
                            continue;
                        const int nx = px + dx;
                        if (nx < 0 || nx >= width)
                            continue;
                        const int next = ny * width + nx;
                        if (!_mask[next])
                            continue;
                        _mask[next]    = 0;
                        _queue[tail++] = (uint16_t)next;
                    }
                }
            }

            if (current.area > best.area)
            {
                best = current;
                memcpy(_bestTopY, _topY, sizeof(_topY));
            }
        }
    }

    return best;
}

void GestureRecognizer::pushHistory(uint8_t centroidX)
{
    if (_historyCount < kWaveWindow)
    {
        _historyX[_historyCount++] = centroidX;
        return;
    }

    for (int i = 1; i < kWaveWindow; i++)
        _historyX[i - 1] = _historyX[i];
    _historyX[kWaveWindow - 1] = centroidX;
}

void GestureRecognizer::clearHistory()
{
    _historyCount = 0;
    memset(_historyX, 0, sizeof(_historyX));
}

bool GestureRecognizer::detectWave(uint8_t *reversalsOut)
{
    *reversalsOut = 0;
    if (_historyCount < kMinHistoryForWave)
        return false;

    uint8_t minCentroid = _historyX[0];
    uint8_t maxCentroid = _historyX[0];
    int reversals       = 0;
    int previousDir     = 0;
    int reference       = _historyX[0];

    for (int i = 1; i < _historyCount; i++)
    {
        if (_historyX[i] < minCentroid)
            minCentroid = _historyX[i];
        if (_historyX[i] > maxCentroid)
            maxCentroid = _historyX[i];

        // 忽略像素级抖动，累积到 2 像素以上再判定方向
        const int delta = (int)_historyX[i] - reference;
        if (delta < 2 && delta > -2)
            continue;

        const int direction = delta > 0 ? 1 : -1;
        if (previousDir != 0 && direction != previousDir)
            reversals++;
        previousDir = direction;
        reference   = _historyX[i];
    }

    *reversalsOut       = (uint8_t)reversals;
    const int amplitude = (int)maxCentroid - (int)minCentroid;
    return reversals >= kWaveMinReversals && amplitude >= kWaveMinAmplitude;
}

uint8_t GestureRecognizer::countFingertips(const uint8_t *topY, const BlobStats &stats)
{
    const int missing = deskquad::kGestureFrameHeight;

    int profile[deskquad::kGestureFrameWidth];
    for (int x = stats.minX; x <= stats.maxX; x++)
        profile[x] = topY[x] == 0xFF ? missing : (int)topY[x];

    int count = 0;
    for (int x = stats.minX + 1; x < stats.maxX; x++)
    {
        if (profile[x] >= missing)
            continue;
        if (profile[x] > profile[x - 1] || profile[x] > profile[x + 1])
            continue;
        // 平台只统计最左侧一次
        if (profile[x] == profile[x - 1] && x > stats.minX + 1)
            continue;

        int leftMax = profile[x];
        for (int k = 1; k <= kTipSearchRadius && x - k >= stats.minX; k++)
            if (profile[x - k] > leftMax)
                leftMax = profile[x - k];

        int rightMax = profile[x];
        for (int k = 1; k <= kTipSearchRadius && x + k <= stats.maxX; k++)
            if (profile[x + k] > rightMax)
                rightMax = profile[x + k];

        const int prominence = (leftMax < rightMax ? leftMax : rightMax) - profile[x];
        if (prominence >= kTipProminence)
            count++;
    }

    return (uint8_t)(count > 9 ? 9 : count);
}

uint8_t GestureRecognizer::countTopColumns(const uint8_t *topY, const BlobStats &stats)
{
    int minTop = deskquad::kGestureFrameHeight;
    for (int x = stats.minX; x <= stats.maxX; x++)
    {
        if (topY[x] != 0xFF && topY[x] < minTop)
            minTop = topY[x];
    }
    if (minTop >= deskquad::kGestureFrameHeight)
        return 0;

    const int boxHeight = stats.maxY - stats.minY + 1;
    int band            = boxHeight / 8;
    if (band < 3)
        band = 3;

    int columns = 0;
    for (int x = stats.minX; x <= stats.maxX; x++)
    {
        if (topY[x] != 0xFF && (int)topY[x] <= minTop + band)
            columns++;
    }
    return (uint8_t)columns;
}

float GestureRecognizer::clampConfidence(float value)
{
    if (value > 0.95f)
        return 0.95f;
    if (value < 0.0f)
        return 0.0f;
    return value;
}
