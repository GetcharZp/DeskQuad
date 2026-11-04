// audio_io.h 封装豆包语音使用的 I2S 麦克风与功放
//
// 麦克风走 I2S_NUM_0（RX），功放走 I2S_NUM_1（TX），两路都是 16kHz。
// 麦克风按 32bit 立体声读取后取左声道高 16bit，避免 INMP441 的 24bit
// 数据被截断；功放直接输出 16bit 单声道 PCM。

#pragma once

#include <Arduino.h>
#include <stddef.h>
#include <stdint.h>

class AudioIo
{
public:
    // Begin 初始化两路 I2S；任意一路失败都返回 false
    bool Begin();

    bool IsReady() const { return _ready; }

    // ReadMic 读取最多 maxSamples 个 16bit 单声道样本
    //
    // # Returns:
    //
    //	实际样本数；0 表示本次没有数据或读取失败
    size_t ReadMic(int16_t *dst, size_t maxSamples, uint32_t timeoutMs);

    // WritePcm 播放 16bit 单声道 PCM，内部按 20ms 分块写入
    bool WritePcm(const int16_t *src, size_t samples);

    // ClearSpeaker 清空功放 DMA，打断时使用
    void ClearSpeaker();

private:
    bool _ready = false;
    int32_t _raw[640] = {}; // 320 个立体声 32bit 样本
};
