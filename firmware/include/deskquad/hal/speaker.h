// speaker.h 旋律提示音模块
//
// 音符被合成为 16kHz / 16bit PCM，通过 I2S 功放 (MAX98357A) 输出。
// VoiceService 初始化音频后调用 SetToneSink 注入输出。
#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include <deskquad/core/states.h>
#include <deskquad/hal/audio_sink.h>

// 音符频率定义 (Hz)
#define NOTE_B0   31
#define NOTE_C1   33
#define NOTE_D1   37
#define NOTE_E1   41
#define NOTE_F1   44
#define NOTE_G1   49
#define NOTE_A1   55
#define NOTE_B1   62
#define NOTE_C2   65
#define NOTE_D2   73
#define NOTE_E2   82
#define NOTE_F2   87
#define NOTE_G2   98
#define NOTE_A2   110
#define NOTE_B2   123
#define NOTE_C3   131
#define NOTE_D3   147
#define NOTE_E3   165
#define NOTE_F3   175
#define NOTE_G3   196
#define NOTE_A3   220
#define NOTE_B3   247
#define NOTE_C4   262
#define NOTE_D4   294
#define NOTE_E4   330
#define NOTE_F4   349
#define NOTE_G4   392
#define NOTE_A4   440
#define NOTE_B4   494
#define NOTE_C5   523
#define NOTE_D5   587
#define NOTE_E5   659
#define NOTE_F5   698
#define NOTE_G5   784
#define NOTE_A5   880
#define NOTE_B5   988
#define NOTE_C6   1047
#define NOTE_D6   1175
#define NOTE_E6   1319
#define NOTE_F6   1397
#define NOTE_G6   1568
#define NOTE_A6   1760
#define NOTE_B6   1976
#define NOTE_C7   2093
#define NOTE_D7   2349
#define NOTE_E7   2637
#define NOTE_F7   2794
#define NOTE_G7   3136
#define NOTE_A7   3520
#define NOTE_B7   3951
#define NOTE_C8   4186
#define NOTE_REST 0 // 休止符

// 音符结构: {频率, 持续时间(ms)}
typedef struct
{
    uint16_t freq;
    uint16_t duration;
} Note;

class Speaker
{
public:
    Speaker() = default;

    // begin 复位播放状态；音频输出由 SetToneSink 在 I2S 就绪后注入
    void begin();

    // SetToneSink 绑定音频输出；传 nullptr 表示静音
    void SetToneSink(ToneSink *sink) { _sink = sink; }

    // 播放指定旋律 (非阻塞)
    void play(Melody melody);

    // 播放自定义音符序列 (非阻塞)
    void playNotes(const Note *notes, uint16_t count);

    // 停止播放
    void stop();

    // 主循环调用 — 处理非阻塞播放
    void loop();

    // 是否正在播放
    bool isPlaying() const { return _isPlaying; }

    // CurrentNoteFreq 返回当前音符频率，静音或休止符返回 0
    //
    // 表情渲染用它驱动眼睛弹跳：音高越高，跳得越快。
    uint16_t CurrentNoteFreq() const;

    // 设置静音
    void setMute(bool mute)
    {
        _mute = mute;
        if (mute)
            stop();
    }
    bool isMuted() const { return _mute; }

private:
    // startNote 切换到新音符：清空功放里上一音符的尾音并复位相位
    void startNote(uint16_t freq);

    // feedTone 生成 count 个样本并写入功放，返回实际写入数量
    size_t feedTone(uint32_t count);

    ToneSink *_sink = nullptr;
    bool _mute       = false;
    bool _isPlaying  = false;

    // 主循环与语音任务都会调用 play/stop/loop，用递归锁保护播放状态
    SemaphoreHandle_t _mutex = nullptr;

    // 非阻塞播放状态
    const Note *_currentNotes = nullptr;
    uint16_t _noteCount       = 0;
    uint16_t _noteIndex       = 0;
    unsigned long _noteStartMs = 0;
    uint32_t _fedSamples       = 0; // 已喂给功放的样本数（相对本次播放起点）

    // 方波相位累加器
    uint32_t _phase     = 0;
    uint32_t _phaseStep = 0;

    // 内置旋律定义
    static const Note _startupMelody[];
    static const Note _commandOkMelody[];
    static const Note _errorMelody[];
    static const Note _helloMelody[];
    static const Note _goodbyeMelody[];
    static const Note _happyMelody[];
    static const Note _sadMelody[];
    static const Note _alertMelody[];
    static const Note _doneMelody[];
    static const Note _beepMelody[];
    static const Note *_melodyMap[];
};
