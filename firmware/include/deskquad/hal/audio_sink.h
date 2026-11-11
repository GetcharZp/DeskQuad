// audio_sink.h 定义旋律提示音的输出接口
//
// Speaker 只负责音符时序与波形生成；具体写到哪里由 ToneSink 决定，
// 当前由 I2S 功放 (AudioIo) 实现。这样 hal/speaker 不依赖 voice/audio_io。

#pragma once

#include <stddef.h>
#include <stdint.h>

class ToneSink
{
public:
    virtual ~ToneSink() = default;

    // WriteTone 非阻塞写入 16kHz / 16bit / 单声道样本
    //
    // # Returns:
    //
    //	实际写入的样本数；0 表示功放未就绪或 DMA 缓冲已满
    virtual size_t WriteTone(const int16_t *samples, size_t count) = 0;

    // ClearTone 丢弃尚未播放的样本，切换音符或停止时调用
    virtual void ClearTone() = 0;

    // IsToneBusy 返回功放是否正被语音 TTS 占用
    virtual bool IsToneBusy() const = 0;
};
