#include <deskquad/voice/voice_service.h>

#include <WiFi.h>
#include <esp_heap_caps.h>
#include <math.h>

#include <deskquad/voice/voice_config.h>

namespace
{
    constexpr size_t kRingSamples = 65536; // 4.096s @ 16kHz，写满时覆盖最旧数据
    constexpr size_t kMicFrame    = 320;   // 20ms
    constexpr size_t kAsrChunk    = 3200;  // 200ms

    bool expired(uint32_t deadline)
    {
        return static_cast<int32_t>(millis() - deadline) >= 0;
    }
} // namespace

bool PcmRing::Begin(size_t capacity)
{
    _capacity = capacity;
    _buffer   = static_cast<int16_t *>(heap_caps_malloc(capacity * sizeof(int16_t), MALLOC_CAP_SPIRAM));
    if (_buffer == nullptr)
    {
        _capacity = 0;
        return false;
    }
    _head = 0;
    _tail = 0;
    return true;
}

void PcmRing::Clear()
{
    portENTER_CRITICAL(&_mux);
    _head = 0;
    _tail = 0;
    portEXIT_CRITICAL(&_mux);
}

void PcmRing::KeepLast(size_t samples)
{
    portENTER_CRITICAL(&_mux);
    if (_head - _tail > samples)
        _tail = _head - static_cast<uint32_t>(samples);
    portEXIT_CRITICAL(&_mux);
}

size_t PcmRing::Write(const int16_t *src, size_t count)
{
    if (_buffer == nullptr || count == 0)
        return 0;

    if (count > _capacity)
    {
        src += count - _capacity;
        count = _capacity;
    }

    portENTER_CRITICAL(&_mux);
    for (size_t i = 0; i < count; i++)
        _buffer[(_head++) & (_capacity - 1)] = src[i];
    if (_head - _tail > _capacity)
        _tail = _head - static_cast<uint32_t>(_capacity);
    portEXIT_CRITICAL(&_mux);
    return count;
}

size_t PcmRing::Read(int16_t *dst, size_t count)
{
    if (_buffer == nullptr || count == 0)
        return 0;

    size_t read = 0;
    portENTER_CRITICAL(&_mux);
    const size_t available = _head - _tail;
    read                   = available < count ? available : count;
    for (size_t i = 0; i < read; i++)
        dst[i] = _buffer[(_tail++) & (_capacity - 1)];
    portEXIT_CRITICAL(&_mux);
    return read;
}

size_t PcmRing::Available()
{
    portENTER_CRITICAL(&_mux);
    const size_t available = _head - _tail;
    portEXIT_CRITICAL(&_mux);
    return available;
}

void VoiceService::Begin(AgentService &agent, RuntimeState &runtime, Speaker &speaker)
{
    _agent   = &agent;
    _runtime = &runtime;
    _speaker = &speaker;

    // 没有 Key 也照常启动，用户可以在网页里补填后再使用
    if (!_hasKey)
        Serial.println("[warn] voice: api key missing, configure it in the web console");

    if (!_audio.Begin() || !_ring.Begin(kRingSamples) || !_ws.Begin())
    {
        _state = State::Error;
        Serial.println("[warn] voice: audio/psram init failed, voice disabled");
        return;
    }

    const BaseType_t micOk =
        xTaskCreatePinnedToCore(MicEntry, "voice_mic", 4096, this, 4, &_micTask, 0);
    const BaseType_t appOk =
        xTaskCreatePinnedToCore(VoiceEntry, "voice_app", 16384, this, 5, &_voiceTask, 1);
    if (micOk != pdPASS || appOk != pdPASS)
    {
        if (_micTask != nullptr)
            vTaskDelete(_micTask);
        if (_voiceTask != nullptr)
            vTaskDelete(_voiceTask);
        _state = State::Error;
        Serial.println("[warn] voice: task create failed");
        return;
    }

    _started = true;
    Serial.println("[sys] voice: doubao asr/tts ready");
}

void VoiceService::SetApiKey(const char *apiKey)
{
    portENTER_CRITICAL(&_keyMux);
    if (apiKey == nullptr)
    {
        _apiKey[0] = '\0';
    }
    else
    {
        strncpy(_apiKey, apiKey, sizeof(_apiKey) - 1);
        _apiKey[sizeof(_apiKey) - 1] = '\0';
    }
    _hasKey = _apiKey[0] != '\0';
    portEXIT_CRITICAL(&_keyMux);
}

void VoiceService::copyApiKey(char *out, size_t capacity) const
{
    if (out == nullptr || capacity == 0)
        return;

    portENTER_CRITICAL(&_keyMux);
    strncpy(out, _apiKey, capacity - 1);
    out[capacity - 1] = '\0';
    portEXIT_CRITICAL(&_keyMux);
}

bool VoiceService::lockWs()
{
    portENTER_CRITICAL(&_wsMux);
    const bool acquired = !_wsBusy;
    if (acquired)
        _wsBusy = true;
    portEXIT_CRITICAL(&_wsMux);
    return acquired;
}

void VoiceService::unlockWs()
{
    portENTER_CRITICAL(&_wsMux);
    _wsBusy = false;
    portEXIT_CRITICAL(&_wsMux);
}

bool VoiceService::TestApiKey(const char *apiKey, String &message)
{
    if (apiKey == nullptr || *apiKey == '\0')
    {
        message = "请先填写豆包 API Key";
        return false;
    }
    // 只测 Key 不依赖麦克风；Error 状态也允许测试，方便定位是配置还是硬件问题
    if (_state != State::Idle && _state != State::Error)
    {
        message = "语音正在使用中，请稍后再试";
        return false;
    }
    if (!_ws.Begin())
    {
        message = "设备 PSRAM 不可用，无法建立安全连接";
        return false;
    }
    if (!lockWs())
    {
        message = "语音正在使用中，请稍后再试";
        return false;
    }

    const bool ok = _tts.Probe(apiKey, message);
    unlockWs();
    return ok;
}

bool VoiceService::IsAvailable() const
{
    return _started && _audio.IsReady();
}

bool VoiceService::IsBusy() const
{
    const State state = _state;
    return state == State::Listening || state == State::Thinking || state == State::Speaking;
}

void VoiceService::Interrupt()
{
    if (_state == State::Idle)
    {
        openConversation();
        if (_speaker != nullptr)
            _speaker->play(MELODY_BEEP);
        return;
    }
    _cancel = true;
}

uint16_t VoiceService::LevelHz() const
{
    return static_cast<uint16_t>(150 + static_cast<uint16_t>(_level) * 8);
}

const char *VoiceService::Status() const
{
    switch (_state)
    {
    case State::Idle:
        return "idle";
    case State::Listening:
        return "listening";
    case State::Thinking:
        return "thinking";
    case State::Speaking:
        return "speaking";
    case State::Error:
        return "error";
    }
    return "unknown";
}

void VoiceService::openConversation()
{
    _conversationOpen  = true;
    _conversationUntil = millis() + deskquad::voice::kConversationWindowMs;
}

void VoiceService::resetInput()
{
    _speechStart = false;
    _speechEnd   = false;
    _resetVad    = true;
    _ring.Clear();
}

bool VoiceService::waitAsrFinal(String &text, String &seen)
{
    const uint32_t deadline = millis() + deskquad::voice::kAsrFinalTimeoutMs;
    while (!expired(deadline) && !_cancel)
    {
        text       = "";
        bool final = false;
        const int result = _asr.Poll(text, final, 200);
        if (text.length() > 0)
            seen = text;
        if (final)
            return true;
        if (result < 0)
            return false;
    }
    return false;
}

bool VoiceService::hasWakeWord(const String &text) const
{
    String lower = text;
    lower.toLowerCase();
    for (const char *word : deskquad::voice::kWakeWords)
    {
        String candidate(word);
        candidate.toLowerCase();
        if (lower.indexOf(candidate) >= 0)
            return true;
    }
    return false;
}

void VoiceService::MicEntry(void *arg)
{
    static_cast<VoiceService *>(arg)->RunMic();
}

void VoiceService::VoiceEntry(void *arg)
{
    static_cast<VoiceService *>(arg)->RunVoice();
}

void VoiceService::RunMic()
{
    int16_t frame[kMicFrame];
    for (;;)
    {
        const State state = _state;
        if (state != State::Idle && state != State::Listening)
        {
            vTaskDelay(pdMS_TO_TICKS(20));
            continue;
        }

        // TTS 刚结束时功放 DMA 还有尾音，先丢 400ms，避免把自己吵醒
        if (state == State::Idle && millis() < _muteUntil)
        {
            _resetVad = true;
            vTaskDelay(pdMS_TO_TICKS(20));
            continue;
        }

        const size_t count = _audio.ReadMic(frame, kMicFrame, 50);
        if (count == 0)
        {
            vTaskDelay(1);
            continue;
        }

        uint64_t sum = 0;
        for (size_t i = 0; i < count; i++)
            sum += static_cast<int32_t>(frame[i]) * static_cast<int32_t>(frame[i]);
        const uint32_t rms = static_cast<uint32_t>(sqrt(static_cast<double>(sum) / count));

        if (_resetVad)
        {
            _resetVad   = false;
            _inSpeech   = false;
            _speechRun  = 0;
            _silenceRun = 0;
        }

        if (_speaking)
        {
            vTaskDelay(1);
            continue;
        }

        _level = static_cast<uint8_t>(rms / 64 > 100 ? 100 : rms / 64);
        _ring.Write(frame, count);

        if (_speechEnd)
            continue;

        const uint32_t startThreshold =
            rms > 0 && _noiseFloor > 0 ? static_cast<uint32_t>(_noiseFloor * 3.0f) : 0;
        const uint32_t endThreshold = static_cast<uint32_t>(_noiseFloor * 1.8f);
        const uint32_t startRms     = startThreshold > deskquad::voice::kStartRmsFloor
                                          ? startThreshold
                                          : deskquad::voice::kStartRmsFloor;
        const uint32_t endRms       = endThreshold > deskquad::voice::kEndRmsFloor
                                          ? endThreshold
                                          : deskquad::voice::kEndRmsFloor;

        if (!_inSpeech)
        {
            _noiseFloor = _noiseFloor * 0.98f + static_cast<float>(rms) * 0.02f;
            if (_noiseFloor < 50.0f)
                _noiseFloor = 50.0f;
            if (_noiseFloor > 4000.0f)
                _noiseFloor = 4000.0f;

            if (rms > startRms)
            {
                if (++_speechRun >= deskquad::voice::kStartFrames)
                {
                    _inSpeech      = true;
                    _speechStart   = true;
                    _speechStartMs = millis();
                    _silenceRun    = 0;
                    _speechRun     = 0;
                }
            }
            else
            {
                _speechRun = 0;
            }
        }
        else
        {
            if (rms < endRms)
            {
                if (++_silenceRun >= deskquad::voice::kSilenceFrames)
                {
                    _inSpeech   = false;
                    _speechEnd  = true;
                    _silenceRun = 0;
                }
            }
            else
            {
                _silenceRun = 0;
            }

            if (millis() - _speechStartMs >= deskquad::voice::kMaxUtteranceMs)
            {
                _inSpeech  = false;
                _speechEnd = true;
            }
        }

        vTaskDelay(1);
    }
}

void VoiceService::RunVoice()
{
    for (;;)
    {
        if (_state != State::Idle)
        {
            vTaskDelay(pdMS_TO_TICKS(20));
            continue;
        }
        if (!_speechStart)
        {
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        _speechStart = false;
        _speechEnd   = false;
        _cancel      = false;

        char apiKey[deskquad::voice::kApiKeyMax];
        copyApiKey(apiKey, sizeof(apiKey));
        if (apiKey[0] == '\0')
        {
            Serial.println("[warn] voice: api key missing, configure it in the web console");
            _state = State::Idle;
            resetInput();
            vTaskDelay(pdMS_TO_TICKS(500));
            continue;
        }

        if (WiFi.status() != WL_CONNECTED)
        {
            Serial.println("[warn] voice: no wifi, skip utterance");
            resetInput();
            vTaskDelay(pdMS_TO_TICKS(500));
            continue;
        }

        _state = State::Listening;
        if (_runtime != nullptr)
        {
            _runtime->lastInteractMs = millis();
            _runtime->voiceTriggered = true;
        }
        _ring.KeepLast(deskquad::voice::kPreRollSamples);

        if (!lockWs())
        {
            // 网页正在测试 Key，这次语音先丢弃，避免共用同一个连接
            _state = State::Idle;
            resetInput();
            vTaskDelay(pdMS_TO_TICKS(200));
            continue;
        }

        if (!_asr.Open(apiKey))
        {
            unlockWs();
            _state = State::Idle;
            resetInput();
            vTaskDelay(pdMS_TO_TICKS(300));
            continue;
        }

        size_t filled = 0;
        String seen;
        String text;
        const uint32_t started = millis();

        while (true)
        {
            if (_cancel)
                break;

            while (filled < kAsrChunk)
            {
                const size_t count = _ring.Read(_asrChunk + filled, kAsrChunk - filled);
                if (count == 0)
                    break;
                filled += count;
            }
            if (filled == kAsrChunk)
            {
                if (!_asr.Send(_asrChunk, kAsrChunk, false))
                    break;
                filled = 0;
            }

            text = "";
            bool final = false;
            const int status = _asr.Poll(text, final, 0);
            if (text.length() > 0)
                seen = text;
            if (final)
                break;
            if (status < 0)
                break;

            if (_speechEnd)
            {
                while (filled < kAsrChunk)
                {
                    const size_t count = _ring.Read(_asrChunk + filled, kAsrChunk - filled);
                    if (count == 0)
                        break;
                    filled += count;
                }
                if (!_asr.Send(filled > 0 ? _asrChunk : nullptr, filled, true))
                    break;

                waitAsrFinal(text, seen);
                break;
            }

            if (millis() - started > deskquad::voice::kMaxUtteranceMs + 2000)
            {
                _asr.Send(filled > 0 ? _asrChunk : nullptr, filled, true);
                waitAsrFinal(text, seen);
                break;
            }

            vTaskDelay(pdMS_TO_TICKS(20));
        }

        _asr.Close();
        unlockWs();

        if (_cancel)
        {
            _cancel = false;
            openConversation();
            _state = State::Idle;
            resetInput();
            vTaskDelay(pdMS_TO_TICKS(200));
            continue;
        }

        const String finalText = text.length() > 0 ? text : seen;
        if (finalText.length() == 0)
        {
            _state = State::Idle;
            resetInput();
            vTaskDelay(pdMS_TO_TICKS(200));
            continue;
        }

        Serial.printf("[voice] asr: %s\n", finalText.c_str());

        const bool windowOpen = _conversationOpen && !expired(_conversationUntil);
        if (!windowOpen && !hasWakeWord(finalText))
        {
            Serial.printf("[voice] ignored, no wake word: %s\n", finalText.c_str());
            _state = State::Idle;
            resetInput();
            vTaskDelay(pdMS_TO_TICKS(200));
            continue;
        }
        if (!windowOpen && _speaker != nullptr)
            _speaker->play(MELODY_BEEP); // 唤醒确认

        String requestId;
        String reply;
        if (_agent != nullptr)
            requestId = _agent->Submit(finalText, false, true);

        if (requestId.length() == 0)
        {
            if (_agent != nullptr && !_agent->IsReady())
                reply = "请先在网页里配置大模型。";
            else
            {
                Serial.println("[warn] voice: agent busy, utterance dropped");
                _state = State::Idle;
                resetInput();
                vTaskDelay(pdMS_TO_TICKS(200));
                continue;
            }
        }
        else
        {
            _state = State::Thinking;
            AgentSnapshot snapshot;
            const uint32_t deadline = millis() + deskquad::voice::kAgentTimeoutMs;
            bool done               = false;
            while (!expired(deadline) && !_cancel)
            {
                snapshot = _agent->Snapshot();
                if (snapshot.requestId == requestId && !snapshot.busy)
                {
                    done = true;
                    break;
                }
                vTaskDelay(pdMS_TO_TICKS(100));
            }

            if (_cancel)
            {
                _cancel = false;
                openConversation();
                _state = State::Idle;
                resetInput();
                vTaskDelay(pdMS_TO_TICKS(200));
                continue;
            }

            if (done && snapshot.status == "completed")
                reply = snapshot.reply;
            else if (done && snapshot.status == "failed")
                reply = "出了点问题，请稍后再试。";
        }

        if (reply.length() == 0)
        {
            _state = State::Idle;
            resetInput();
            vTaskDelay(pdMS_TO_TICKS(200));
            continue;
        }

        if (_speaker != nullptr)
            _speaker->stop();

        if (!lockWs())
        {
            // 网页正在测试 Key，这条回复无法播放
            _state = State::Idle;
            resetInput();
            vTaskDelay(pdMS_TO_TICKS(200));
            continue;
        }

        _state    = State::Speaking;
        _speaking = true;
        _cancel   = false;
        const bool spoken = _tts.Speak(apiKey, reply.c_str(), _audio, _cancel, _level);
        _speaking         = false;
        unlockWs();
        _muteUntil = millis() + 400;

        if (!spoken && !_cancel && _speaker != nullptr)
            _speaker->play(MELODY_ERROR);

        _cancel = false;
        _state  = State::Idle;
        openConversation();
        resetInput();
        vTaskDelay(pdMS_TO_TICKS(250));
    }
}
