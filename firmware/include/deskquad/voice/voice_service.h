// voice_service.h 把麦克风、豆包 ASR/TTS 和现有 Agent 串成一条语音链路
//
// 任务划分：
//   voice_mic  持续读 I2S，做能量 VAD，把 PCM 写进 PSRAM 环形缓冲
//   voice_app  在说话时把缓冲推给 ASR，拿到文本后交给 AgentService，
//              再把回复文本交给 TTS 并播放
//
// 两个任务通过 volatile 标志和环形缓冲协作，不共享网络/TLS 对象。

#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <deskquad/app/agent_service.h>
#include <deskquad/app/runtime_state.h>
#include <deskquad/hal/speaker.h>
#include <deskquad/voice/audio_io.h>
#include <deskquad/voice/doubao.h>
#include <deskquad/voice/voice_config.h>
#include <deskquad/voice/ws_client.h>

// PcmRing 是麦克风任务到语音任务之间的单生产者/单消费者缓冲
//
// 写满时覆盖最旧的数据，保证触发后总能拿到最近一段音频。
class PcmRing
{
public:
    bool Begin(size_t capacity);
    void Clear();
    void KeepLast(size_t samples);
    size_t Write(const int16_t *src, size_t count);
    size_t Read(int16_t *dst, size_t count);
    size_t Available();

private:
    int16_t *_buffer  = nullptr;
    size_t _capacity  = 0;
    uint32_t _head    = 0;
    uint32_t _tail    = 0;
    portMUX_TYPE _mux = portMUX_INITIALIZER_UNLOCKED;
};

class VoiceService
{
public:
    // Begin 初始化音频、缓冲并启动任务
    void Begin(AgentService &agent, RuntimeState &runtime, Speaker &speaker);

    // SetApiKey 更新豆包 API Key；下次语音会话立即使用
    void SetApiKey(const char *apiKey);

    bool HasApiKey() const { return _hasKey; }

    // TestApiKey 只用握手校验豆包 API Key，不合成也不播放
    //
    // # Returns:
    //
    //	true 表示连接正常；false 时 message 给出可直接展示的原因
    bool TestApiKey(const char *apiKey, String &message);

    bool IsAvailable() const;
    bool IsSpeaking() const { return _speaking; }

    // IsBusy 包含识别、思考和播放三个阶段
    bool IsBusy() const;

    // Interrupt 停止当前 TTS/ASR；空闲时打开一次连续对话窗口
    void Interrupt();

    // LevelHz 把当前播放/录音能量映射成表情驱动的“音高”
    uint16_t LevelHz() const;

    const char *Status() const;

private:
    enum class State : uint8_t
    {
        Idle,
        Listening,
        Thinking,
        Speaking,
        Error,
    };

    static void MicEntry(void *arg);
    static void VoiceEntry(void *arg);
    void RunMic();
    void RunVoice();

    void resetInput();
    bool waitAsrFinal(String &text, String &seen);
    bool hasWakeWord(const String &text) const;
    void openConversation();

    // copyApiKey 把当前 Key 复制到调用方缓冲区，避免任务之间共享 String
    void copyApiKey(char *out, size_t capacity) const;

    // lockWs/unlockWs 保证语音任务与网页测试不会同时使用同一个 WsClient
    bool lockWs();
    void unlockWs();

    AgentService *_agent   = nullptr;
    RuntimeState *_runtime = nullptr;
    Speaker *_speaker      = nullptr;

    AudioIo _audio;
    WsClient _ws;
    deskquad::voice::DoubaoAsr _asr{_ws};
    deskquad::voice::DoubaoTts _tts{_ws};
    PcmRing _ring;

    TaskHandle_t _micTask   = nullptr;
    TaskHandle_t _voiceTask = nullptr;
    bool _started           = false;

    // 200ms @ 16kHz，ASR 分片缓冲
    int16_t _asrChunk[3200] = {};

    volatile State _state = State::Idle;
    volatile bool _speaking = false;
    volatile bool _cancel   = false;
    volatile bool _speechStart = false;
    volatile bool _speechEnd   = false;
    volatile bool _resetVad    = false;
    volatile uint8_t _level    = 0;

    volatile bool _conversationOpen          = false;
    volatile uint32_t _conversationUntil     = 0;
    volatile uint32_t _muteUntil             = 0; // TTS 结束后丢弃功放回声的时间点

    // API Key 由网页线程写入、语音任务读取，用临界区保护固定缓冲区
    char _apiKey[deskquad::voice::kApiKeyMax] = {};
    volatile bool _hasKey                     = false;
    mutable portMUX_TYPE _keyMux              = portMUX_INITIALIZER_UNLOCKED;

    // 同一时刻只允许一个任务使用 _ws（语音链路或网页测试）
    volatile bool _wsBusy         = false;
    portMUX_TYPE _wsMux           = portMUX_INITIALIZER_UNLOCKED;

    // 只由麦克风任务访问
    float _noiseFloor    = 300.0f;
    uint16_t _speechRun  = 0;
    uint16_t _silenceRun = 0;
    bool _inSpeech       = false;
    uint32_t _speechStartMs = 0;
};
