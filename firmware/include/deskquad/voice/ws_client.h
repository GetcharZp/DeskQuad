// ws_client.h 是最小 WebSocket 客户端
//
// 只实现豆包语音需要的部分：TLS 握手、带自定义 header 的 Upgrade、二进制
// 帧收发、ping/pong 和分片重组。接收缓冲放在 PSRAM，单条消息上限 96KB，
// 足够覆盖 TTS 的音频分片。

#pragma once

#include <Arduino.h>
#include <WiFiClientSecure.h>

class WsClient
{
public:
    WsClient() = default;

    // Begin 分配接收缓冲；PSRAM 不可用时返回 false
    bool Begin();

    // Connect 建立 TLS 连接并完成 WebSocket Upgrade
    //
    // # Params:
    //
    //	headers: 以 \r\n 结尾的额外请求头，如 X-Api-Key
    bool Connect(const char *host, uint16_t port, const char *path, const String &headers);

    void Close();
    bool Connected() { return _client.connected(); }
    bool HasData() { return _client.available() > 0; }

    bool SendBinary(const uint8_t *data, size_t len);

    // Receive 读取一条完整的二进制/文本消息
    //
    // # Returns:
    //
    //	>0 消息长度；0 超时；-1 连接或协议错误
    int Receive(uint32_t timeoutMs);

    const uint8_t *Data() const { return _rx; }
    size_t Length() const { return _rxLen; }
    const String &Error() const { return _error; }

private:
    bool handshake(const char *host, const char *path, const String &headers);
    bool sendFrame(uint8_t opcode, const uint8_t *data, size_t len);
    bool writeAll(const uint8_t *data, size_t len);
    bool readExact(uint8_t *dst, size_t len, uint32_t deadline);
    bool fail(const char *message);

    WiFiClientSecure _client;
    uint8_t *_rx     = nullptr;
    size_t _rxCap    = 0;
    size_t _rxLen    = 0;
    uint8_t _chunk[256] = {};
    String _error;
};
