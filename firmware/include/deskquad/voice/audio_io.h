// audio_io.h 封装豆包语音使用的 I2S 麦克风与功放
//
// 麦克风走 I2S_NUM_0（RX），功放走 I2S_NUM_1（TX），两路都是 16kHz。
// 麦克风按 32bit 立体声读取后取左声道高 16bit，避免 INMP441 的 24bit
// 数据被截断；功放直接输出 16bit 单声道 PCM。
//
// 功放同时承担旋律提示音输出，AudioIo 因此实现 ToneSink。

#pragma once

#include <Arduino.h>
#include <stddef.h>
#include <stdint.h>

#include <deskquad/hal/audio_sink.h>

class AudioIo : public ToneSink
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

    // SetTtsActive 标记语音 TTS 是否正在占用功放
    //
    // TTS 期间 Speaker 不再向功放写入提示音，避免两路 PCM 交错。
    void SetTtsActive(bool active) { _ttsActive = active; }

    // ToneSink 接口
    size_t WriteTone(const int16_t *samples, size_t count) override;
    void ClearTone() override;
    bool IsToneBusy() const override { return _ttsActive; }

private:
    bool _ready = false;
    volatile bool _ttsActive = false;
    int32_t _raw[640] = {}; // 320 个立体声 32bit 样本
};
