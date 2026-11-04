#include <deskquad/voice/doubao.h>

#include <ArduinoJson.h>
#include <esp_random.h>

#include <algorithm>

#include <deskquad/voice/voice_config.h>

namespace deskquad::voice
{
    namespace
    {
        bool expired(uint32_t deadline)
        {
            return static_cast<int32_t>(millis() - deadline) >= 0;
        }

        void putU32(uint8_t *out, uint32_t value)
        {
            out[0] = static_cast<uint8_t>(value >> 24);
            out[1] = static_cast<uint8_t>(value >> 16);
            out[2] = static_cast<uint8_t>(value >> 8);
            out[3] = static_cast<uint8_t>(value);
        }

        uint32_t getU32(const uint8_t *in)
        {
            return (static_cast<uint32_t>(in[0]) << 24) | (static_cast<uint32_t>(in[1]) << 16) |
                   (static_cast<uint32_t>(in[2]) << 8) | static_cast<uint32_t>(in[3]);
        }

        bool carriesSessionId(int32_t event)
        {
            return event != 1 && event != 2 && event != 50 && event != 51 && event != 52;
        }

        bool carriesConnectId(int32_t event)
        {
            return event == 50 || event == 51 || event == 52;
        }
    } // namespace

    void MakeUuid(char out[37])
    {
        snprintf(out, 37, "%08x-%04x-%04x-%04x-%08x%04x", static_cast<unsigned>(esp_random()),
                 static_cast<unsigned>(esp_random() & 0xFFFF), static_cast<unsigned>(esp_random() & 0xFFFF),
                 static_cast<unsigned>(esp_random() & 0xFFFF), static_cast<unsigned>(esp_random()),
                 static_cast<unsigned>(esp_random() & 0xFFFF));
    }

    size_t BuildMessage(uint8_t *out, size_t capacity, uint8_t type, uint8_t flags, int32_t event,
                        const char *sessionId, const void *payload, size_t payloadLen, bool json)
    {
        const size_t sessionLen = (flags & kFlagWithEvent) && carriesSessionId(event) && sessionId != nullptr
                                      ? strlen(sessionId)
                                      : 0;
        size_t need = 4 + 4 + payloadLen;
        if (flags & kFlagWithEvent)
            need += 4 + ((flags & kFlagWithEvent) && carriesSessionId(event) ? 4 + sessionLen : 0);
        if (need > capacity)
            return 0;

        uint8_t *cursor = out;
        *cursor++       = 0x11; // version 1, 4 byte header
        *cursor++       = static_cast<uint8_t>((type << 4) | flags);
        *cursor++       = static_cast<uint8_t>((json ? 0x1 : 0x0) << 4); // serialization, no compression
        *cursor++       = 0x00;

        if (flags & kFlagWithEvent)
        {
            putU32(cursor, static_cast<uint32_t>(event));
            cursor += 4;
            if (carriesSessionId(event))
            {
                putU32(cursor, static_cast<uint32_t>(sessionLen));
                cursor += 4;
                if (sessionLen > 0)
                {
                    memcpy(cursor, sessionId, sessionLen);
                    cursor += sessionLen;
                }
            }
        }

        putU32(cursor, static_cast<uint32_t>(payloadLen));
        cursor += 4;
        if (payloadLen > 0 && payload != nullptr)
        {
            memcpy(cursor, payload, payloadLen);
            cursor += payloadLen;
        }
        return static_cast<size_t>(cursor - out);
    }

    bool ParseMessage(const uint8_t *data, size_t length, ParsedMessage &out)
    {
        if (data == nullptr || length < 4)
            return false;

        out              = ParsedMessage{};
        out.type         = static_cast<uint8_t>(data[1] >> 4);
        out.flags        = static_cast<uint8_t>(data[1] & 0x0F);
        out.serialization = static_cast<uint8_t>(data[2] >> 4);
        out.compression  = static_cast<uint8_t>(data[2] & 0x0F);
        const size_t headerSize = static_cast<size_t>(data[0] & 0x0F) * 4;
        if (headerSize < 4 || headerSize > length)
            return false;

        size_t offset = headerSize;
        if (out.flags & kFlagWithEvent)
        {
            if (offset + 4 > length)
                return false;
            out.event = static_cast<int32_t>(getU32(data + offset));
            offset += 4;

            if (carriesSessionId(out.event))
            {
                if (offset + 4 > length)
                    return false;
                const uint32_t size = getU32(data + offset);
                offset += 4;
                if (offset + size > length)
                    return false;
                out.sessionId = String(reinterpret_cast<const char *>(data + offset), size);
                offset += size;
            }
            if (carriesConnectId(out.event))
            {
                if (offset + 4 > length)
                    return false;
                const uint32_t size = getU32(data + offset);
                offset += 4;
                if (offset + size > length)
                    return false;
                out.connectId = String(reinterpret_cast<const char *>(data + offset), size);
                offset += size;
            }
        }

        if (out.flags == 0x1 || out.flags == kFlagNegative)
        {
            if (offset + 4 > length)
                return false;
            out.sequence = static_cast<int32_t>(getU32(data + offset));
            offset += 4;
        }

        if (offset + 4 > length)
            return false;
        const uint32_t payloadLen = getU32(data + offset);
        offset += 4;
        if (offset + payloadLen > length)
            return false;
        out.payload    = data + offset;
        out.payloadLen = payloadLen;

        if (out.compression != 0)
        {
            Serial.printf("[warn] voice: compressed frame (%u) unsupported\n", out.compression);
            return false;
        }
        return true;
    }

    bool DoubaoAsr::Open(const char *apiKey)
    {
        if (apiKey == nullptr || *apiKey == '\0')
        {
            Serial.println("[warn] voice: asr api key missing");
            return false;
        }

        char requestId[37];
        MakeUuid(requestId);

        String headers;
        headers.reserve(192);
        headers += "X-Api-Key: ";
        headers += apiKey;
        headers += "\r\nX-Api-Resource-Id: ";
        headers += kAsrResourceId;
        headers += "\r\nX-Api-Request-Id: ";
        headers += requestId;
        headers += "\r\nX-Api-Sequence: -1\r\n";

        if (!_ws.Connect(kHost, kPort, kAsrPath, headers))
        {
            Serial.printf("[warn] voice: asr connect failed: %s\n", _ws.Error().c_str());
            return false;
        }

        JsonDocument doc;
        doc["user"]["uid"]          = "deskquad";
        doc["audio"]["format"]      = "pcm";
        doc["audio"]["codec"]       = "raw";
        doc["audio"]["rate"]        = kSampleRate;
        doc["audio"]["bits"]        = 16;
        doc["audio"]["channel"]     = 1;
        doc["request"]["model_name"] = "bigmodel";
        doc["request"]["enable_itn"] = true;
        doc["request"]["enable_punc"] = true;
        doc["request"]["show_utterances"] = true;
        doc["request"]["enable_nonstream"] = true;
        doc["request"]["end_window_size"]  = 800;
        doc["request"]["force_to_speech_time"] = 1000;

        char json[512];
        const size_t jsonLen = serializeJson(doc, json, sizeof(json));
        const size_t frame   = BuildMessage(_tx, sizeof(_tx), kFullClient, kFlagNone, 0, nullptr, json, jsonLen, true);
        if (frame == 0 || !_ws.SendBinary(_tx, frame))
        {
            _ws.Close();
            return false;
        }
        return true;
    }

    bool DoubaoAsr::Send(const int16_t *samples, size_t count, bool last)
    {
        const size_t bytes = count * sizeof(int16_t);
        const size_t frame =
            BuildMessage(_tx, sizeof(_tx), kAudioOnly, last ? kFlagLast : kFlagNone, 0, nullptr, samples, bytes, false);
        return frame != 0 && _ws.SendBinary(_tx, frame);
    }

    int DoubaoAsr::Poll(String &text, bool &final, uint32_t timeoutMs)
    {
        const int length = _ws.Receive(timeoutMs);
        if (length == 0)
            return 0;
        if (length < 0)
            return -1;

        ParsedMessage msg;
        if (!ParseMessage(_ws.Data(), static_cast<size_t>(length), msg))
            return -1;
        if (msg.type == kErrorServer)
        {
            Serial.printf("[warn] voice: asr error len=%u\n", static_cast<unsigned>(msg.payloadLen));
            return -1;
        }
        if (msg.type != kFullServer || msg.payloadLen == 0)
            return 0;

        JsonDocument doc;
        if (deserializeJson(doc, reinterpret_cast<const char *>(msg.payload), msg.payloadLen) != DeserializationError::Ok)
            return 0;

        const char *value = doc["result"]["text"] | "";
        if (value == nullptr || *value == '\0')
            value = doc["payload_msg"]["result"]["text"] | "";
        if (value != nullptr && *value != '\0')
            text = value;

        const bool isFinal = msg.flags == kFlagNegative || doc["is_last_package"].as<bool>();
        final              = isFinal;
        return isFinal ? 1 : 0;
    }

    bool DoubaoTts::sendEvent(int32_t event, const char *sessionId, const char *payload, bool json)
    {
        const size_t length = BuildMessage(_tx, sizeof(_tx), kFullClient, kFlagWithEvent, event, sessionId, payload,
                                           payload != nullptr ? strlen(payload) : 0, json);
        return length != 0 && _ws.SendBinary(_tx, length);
    }

    bool DoubaoTts::waitEvent(int32_t event, uint32_t timeoutMs)
    {
        const uint32_t deadline = millis() + timeoutMs;
        while (!expired(deadline))
        {
            const int length = _ws.Receive(500);
            if (length == 0)
                continue;
            if (length < 0)
                return false;

            ParsedMessage msg;
            if (!ParseMessage(_ws.Data(), static_cast<size_t>(length), msg))
                return false;
            if (msg.type == kErrorServer)
            {
                Serial.printf("[warn] voice: tts error event=%d len=%u\n", msg.event,
                              static_cast<unsigned>(msg.payloadLen));
                return false;
            }
            if (msg.type == kFullServer && msg.event == event)
                return true;
            if (msg.type == kFullServer && (msg.event == 51 || msg.event == 153))
                return false;
        }
        return false;
    }

    void DoubaoTts::cancelSession()
    {
        if (_ws.Connected())
        {
            sendEvent(101, _sessionId, "{}", true); // CancelSession
            sendEvent(2, nullptr, "{}", true);      // FinishConnection
        }
        _ws.Close();
    }

    bool DoubaoTts::openConnection(const char *apiKey)
    {
        if (apiKey == nullptr || *apiKey == '\0')
        {
            Serial.println("[warn] voice: tts api key missing");
            return false;
        }

        char connectId[37];
        MakeUuid(connectId);

        String headers;
        headers.reserve(192);
        headers += "X-Api-Key: ";
        headers += apiKey;
        headers += "\r\nX-Api-Resource-Id: ";
        headers += kTtsResourceId;
        headers += "\r\nX-Api-Connect-Id: ";
        headers += connectId;
        headers += "\r\nX-Control-Require-Usage-Tokens-Return: *\r\n";

        return _ws.Connect(kHost, kPort, kTtsPath, headers);
    }

    bool DoubaoTts::Probe(const char *apiKey, String &error)
    {
        error = "";

        if (!openConnection(apiKey))
        {
            // 握手失败时 WsClient 会保留服务端响应，通常首行就是 401/403
            String reason = _ws.Error();
            if (reason.length() == 0)
            {
                error = "无法连接豆包语音服务，请检查设备网络";
            }
            else
            {
                const int end = reason.indexOf('\r');
                if (end > 0)
                    reason = reason.substring(0, end);
                error = "连接失败：" + reason;
            }
            _ws.Close();
            return false;
        }

        if (!sendEvent(1, nullptr, "{}", true)) // StartConnection
        {
            _ws.Close();
            error = "发送握手请求失败";
            return false;
        }

        const uint32_t deadline = millis() + 8000;
        while (!expired(deadline))
        {
            const int length = _ws.Receive(500);
            if (length == 0)
                continue;
            if (length < 0)
            {
                _ws.Close();
                error = "豆包语音服务未应答，请稍后重试";
                return false;
            }

            ParsedMessage msg;
            if (!ParseMessage(_ws.Data(), static_cast<size_t>(length), msg))
                continue;

            if (msg.type == kErrorServer)
            {
                String detail;
                if (msg.payload != nullptr && msg.payloadLen > 0)
                    detail = String(reinterpret_cast<const char *>(msg.payload), msg.payloadLen);
                detail.trim();
                if (detail.length() > 160)
                    detail = detail.substring(0, 160) + "...";
                _ws.Close();
                if (detail.length() > 0)
                    error = String("豆包返回错误：") + detail;
                else
                    error = "API Key 无效或未开通语音服务";
                return false;
            }
            if (msg.type == kFullServer && msg.event == 50) // ConnectionStarted
            {
                sendEvent(2, nullptr, "{}", true); // FinishConnection
                _ws.Close();
                error = "豆包语音服务连接正常";
                return true;
            }
            if (msg.type == kFullServer && msg.event == 51) // ConnectionFailed
            {
                _ws.Close();
                error = "豆包语音服务拒绝了本次连接";
                return false;
            }
        }

        _ws.Close();
        error = "等待豆包语音服务应答超时";
        return false;
    }

    bool DoubaoTts::Speak(const char *apiKey, const char *text, AudioIo &audio, const volatile bool &cancel,
                          volatile uint8_t &level)
    {
        if (!openConnection(apiKey))
        {
            Serial.printf("[warn] voice: tts connect failed: %s\n", _ws.Error().c_str());
            return false;
        }

        if (!sendEvent(1, nullptr, "{}", true) || !waitEvent(50, kTtsTimeoutMs))
        {
            _ws.Close();
            return false;
        }

        MakeUuid(_sessionId);
        JsonDocument session;
        session["req_params"]["speaker"]                  = kSpeaker;
        session["req_params"]["audio_params"]["format"]   = "pcm";
        session["req_params"]["audio_params"]["sample_rate"] = kSampleRate;

        char sessionJson[256];
        const size_t sessionLen = serializeJson(session, sessionJson, sizeof(sessionJson));
        if (!sendEvent(100, _sessionId, sessionJson, true) || !waitEvent(150, kTtsTimeoutMs))
        {
            _ws.Close();
            return false;
        }

        JsonDocument task;
        task["req_params"]["text"] = text;
        char taskJson[512];
        const size_t taskLen = serializeJson(task, taskJson, sizeof(taskJson));
        if (!sendEvent(200, _sessionId, taskJson, true) || !sendEvent(102, _sessionId, "{}", true))
        {
            _ws.Close();
            return false;
        }

        const uint32_t deadline = millis() + kTtsTimeoutMs;
        while (!expired(deadline))
        {
            if (cancel)
            {
                cancelSession();
                audio.ClearSpeaker();
                level = 0;
                return false;
            }

            const int length = _ws.Receive(500);
            if (length == 0)
                continue;
            if (length < 0)
                return false;

            ParsedMessage msg;
            if (!ParseMessage(_ws.Data(), static_cast<size_t>(length), msg))
                return false;

            if (msg.type == kErrorServer)
            {
                Serial.printf("[warn] voice: tts error event=%d len=%u\n", msg.event,
                              static_cast<unsigned>(msg.payloadLen));
                return false;
            }
            if (msg.type == kFullServer && (msg.event == 51 || msg.event == 153))
                return false;
            if (msg.type == kFullServer && msg.event == 152)
            {
                sendEvent(2, nullptr, "{}", true);
                _ws.Close();
                level = 0;
                return true;
            }
            if (msg.type != kAudioServer || msg.event != 352 || msg.payloadLen < 2)
                continue;

            const int16_t *pcm = reinterpret_cast<const int16_t *>(msg.payload);
            const size_t samples = msg.payloadLen / 2;

            int16_t peak = 0;
            for (size_t i = 0; i < samples; i += 8)
            {
                const int16_t sample = pcm[i];
                const int16_t value  = sample < 0 ? static_cast<int16_t>(-sample) : sample;
                if (value > peak)
                    peak = value;
            }
            level = static_cast<uint8_t>(std::min<int>(100, peak / 80));

            for (size_t offset = 0; offset < samples; offset += 320)
            {
                if (cancel)
                {
                    cancelSession();
                    audio.ClearSpeaker();
                    level = 0;
                    return false;
                }
                const size_t count = std::min<size_t>(320, samples - offset);
                if (!audio.WritePcm(pcm + offset, count))
                    return false;
            }
        }

        cancelSession();
        level = 0;
        return false;
    }

} // namespace deskquad::voice
