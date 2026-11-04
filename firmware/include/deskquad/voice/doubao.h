// doubao.h 实现豆包实时语音的 SAUC 二进制协议
//
// ASR 与 TTS 都走 WebSocket + 自定义二进制帧。这里只保留客户端需要的
// 编码/解码逻辑，消息体统一不压缩（服务端也按未压缩返回），省掉 gzip
// 解码和额外的内存占用。

#pragma once

#include <Arduino.h>
#include <stddef.h>
#include <stdint.h>

#include <deskquad/voice/audio_io.h>
#include <deskquad/voice/ws_client.h>

namespace deskquad::voice
{

    constexpr uint8_t kFullClient    = 0x1;
    constexpr uint8_t kAudioOnly     = 0x2;
    constexpr uint8_t kFullServer    = 0x9;
    constexpr uint8_t kAudioServer   = 0xB;
    constexpr uint8_t kErrorServer   = 0xF;

    constexpr uint8_t kFlagNone      = 0x0;
    constexpr uint8_t kFlagWithEvent = 0x4;
    constexpr uint8_t kFlagLast      = 0x2;
    constexpr uint8_t kFlagNegative  = 0x3;

    struct ParsedMessage
    {
        uint8_t type          = 0;
        uint8_t flags         = 0;
        uint8_t serialization = 0;
        uint8_t compression   = 0;
        int32_t event         = 0;
        int32_t sequence      = 0;
        const uint8_t *payload = nullptr;
        size_t payloadLen      = 0;
        String sessionId;
        String connectId;
    };

    // BuildMessage 组装一帧 SAUC 消息
    //
    // # Returns:
    //
    //	写入的字节数；缓冲区不足时返回 0
    size_t BuildMessage(uint8_t *out, size_t capacity, uint8_t type, uint8_t flags, int32_t event,
                        const char *sessionId, const void *payload, size_t payloadLen, bool json);

    // ParseMessage 解析服务端返回的一帧
    bool ParseMessage(const uint8_t *data, size_t length, ParsedMessage &out);

    // MakeUuid 生成格式合法的随机 UUID 字符串
    void MakeUuid(char out[37]);

    // DoubaoAsr 管理一次流式识别
    class DoubaoAsr
    {
    public:
        explicit DoubaoAsr(WsClient &ws) : _ws(ws) {}

        // Open 建立一次识别会话；apiKey 为空时直接失败
        bool Open(const char *apiKey);
        bool Send(const int16_t *samples, size_t count, bool last);

        // Poll 读取一帧识别结果
        //
        // # Returns:
        //
        //	1 最终结果；0 暂无结果；-1 失败
        int Poll(String &text, bool &final, uint32_t timeoutMs);

        void Close() { _ws.Close(); }

    private:
        WsClient &_ws;
        uint8_t _tx[8 * 1024] = {};
    };

    // DoubaoTts 管理一次语音合成
    class DoubaoTts
    {
    public:
        explicit DoubaoTts(WsClient &ws) : _ws(ws) {}

        // Speak 合成并播放一段文本；cancel 置位时立即停止
        bool Speak(const char *apiKey, const char *text, AudioIo &audio, const volatile bool &cancel,
                   volatile uint8_t &level);

        // Probe 只用握手校验 API Key，不合成也不播放
        //
        // 成功时返回 true；失败时把可直接展示给用户的原因写入 error。
        bool Probe(const char *apiKey, String &error);

    private:
        bool openConnection(const char *apiKey);
        bool sendEvent(int32_t event, const char *sessionId, const char *payload, bool json);
        bool waitEvent(int32_t event, uint32_t timeoutMs);
        void cancelSession();

        WsClient &_ws;
        char _sessionId[37] = {};
        uint8_t _tx[1024]   = {};
    };

} // namespace deskquad::voice
