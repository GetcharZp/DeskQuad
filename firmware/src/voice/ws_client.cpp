#include <deskquad/voice/ws_client.h>

#include <esp_heap_caps.h>
#include <esp_random.h>
#include <mbedtls/base64.h>

namespace
{
    constexpr size_t kRxCapacity = 96 * 1024;
    constexpr size_t kMaxHandshake = 1024;

    bool expired(uint32_t deadline)
    {
        return static_cast<int32_t>(millis() - deadline) >= 0;
    }
} // namespace

bool WsClient::Begin()
{
    if (_rx != nullptr)
        return true;

    _rxCap = kRxCapacity;
    _rx    = static_cast<uint8_t *>(heap_caps_malloc(_rxCap, MALLOC_CAP_SPIRAM));
    if (_rx == nullptr)
    {
        _error = "no psram for websocket";
        _rxCap = 0;
        return false;
    }
    return true;
}

bool WsClient::fail(const char *message)
{
    _error = message;
    return false;
}

bool WsClient::Connect(const char *host, uint16_t port, const char *path, const String &headers)
{
    Close();
    _error = "";
    if (!Begin())
        return false;

    _client.setInsecure(); // 与现有 DeepSeek 客户端保持一致；生产环境应替换为根证书
    _client.setHandshakeTimeout(10);
    _client.setTimeout(10);
    if (!_client.connect(host, port))
        return fail("websocket tcp connect failed");

    return handshake(host, path, headers);
}

bool WsClient::handshake(const char *host, const char *path, const String &headers)
{
    uint8_t random[16];
    for (uint8_t &byte : random)
        byte = static_cast<uint8_t>(esp_random());

    unsigned char key[32] = {};
    size_t keyLen         = 0;
    if (mbedtls_base64_encode(key, sizeof(key), &keyLen, random, sizeof(random)) != 0)
        return fail("websocket key encode failed");
    key[keyLen] = '\0';

    String request;
    request.reserve(512);
    request += "GET ";
    request += path;
    request += " HTTP/1.1\r\nHost: ";
    request += host;
    request += "\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Key: ";
    request += reinterpret_cast<char *>(key);
    request += "\r\nSec-WebSocket-Version: 13\r\n";
    request += headers;
    request += "\r\n";

    if (!writeAll(reinterpret_cast<const uint8_t *>(request.c_str()), request.length()))
        return fail("websocket handshake write failed");

    // 服务端响应头可能超过 kMaxHandshake（豆包的 X-Tt-Trace 系列头就有 200 多
    // 字节）。缓冲区只用来保存错误信息，但必须一直读到 \r\n\r\n，否则剩余的头
    // 字段会留在 TCP 缓冲里，被后面的 Receive 当成 WebSocket 帧解析。
    char response[kMaxHandshake] = {};
    constexpr char kHeaderEnd[]  = "\r\n\r\n";
    size_t length                = 0;
    size_t matched               = 0;
    bool complete                = false;
    const uint32_t deadline      = millis() + 10000;
    while (!expired(deadline))
    {
        if (_client.available() == 0)
        {
            delay(1);
            continue;
        }
        const int value = _client.read();
        if (value < 0)
        {
            delay(1);
            continue;
        }

        if (length < sizeof(response) - 1)
            response[length++] = static_cast<char>(value);

        if (value == kHeaderEnd[matched])
        {
            if (++matched == sizeof(kHeaderEnd) - 1)
            {
                complete = true;
                break;
            }
        }
        else
        {
            matched = value == '\r' ? 1 : 0;
        }
    }
    response[length] = '\0';

    if (!complete)
    {
        _error = "websocket handshake timeout";
        _client.stop();
        return false;
    }

    if (strstr(response, " 101 ") == nullptr)
    {
        _error = response;
        _client.stop();
        return false;
    }
    return true;
}

void WsClient::Close()
{
    if (_client.connected())
    {
        const uint8_t code[2] = {0x03, 0xE8}; // 1000 normal closure
        sendFrame(0x8, code, sizeof(code));
    }
    _client.stop();
    _rxLen = 0;
}

bool WsClient::writeAll(const uint8_t *data, size_t len)
{
    size_t offset = 0;
    while (offset < len)
    {
        const size_t written = _client.write(data + offset, len - offset);
        if (written == 0)
        {
            if (!_client.connected())
                return false;
            delay(1);
            continue;
        }
        offset += written;
    }
    return true;
}

bool WsClient::sendFrame(uint8_t opcode, const uint8_t *data, size_t len)
{
    uint8_t header[14] = {};
    size_t headerLen   = 2;
    header[0]          = static_cast<uint8_t>(0x80 | opcode);
    if (len < 126)
    {
        header[1] = static_cast<uint8_t>(0x80 | len);
    }
    else if (len < 65536)
    {
        header[1] = 0x80 | 126;
        header[2] = static_cast<uint8_t>(len >> 8);
        header[3] = static_cast<uint8_t>(len);
        headerLen = 4;
    }
    else
    {
        header[1] = 0x80 | 127;
        for (uint8_t i = 0; i < 8; i++)
            header[2 + i] = static_cast<uint8_t>(len >> (56 - i * 8));
        headerLen = 10;
    }

    uint8_t mask[4];
    for (uint8_t &byte : mask)
        byte = static_cast<uint8_t>(esp_random());
    memcpy(header + headerLen, mask, sizeof(mask));
    headerLen += sizeof(mask);

    if (!writeAll(header, headerLen))
        return false;

    size_t offset = 0;
    while (offset < len)
    {
        const size_t count = len - offset > sizeof(_chunk) ? sizeof(_chunk) : len - offset;
        for (size_t i = 0; i < count; i++)
            _chunk[i] = data[offset + i] ^ mask[(offset + i) & 3];
        if (!writeAll(_chunk, count))
            return false;
        offset += count;
    }
    return true;
}

bool WsClient::SendBinary(const uint8_t *data, size_t len)
{
    return sendFrame(0x2, data, len);
}

bool WsClient::readExact(uint8_t *dst, size_t len, uint32_t deadline)
{
    size_t offset = 0;
    while (offset < len)
    {
        if (!_client.connected() && _client.available() == 0)
            return false;
        const int value = _client.read(dst + offset, len - offset);
        if (value > 0)
        {
            offset += static_cast<size_t>(value);
            continue;
        }
        if (expired(deadline))
            return false;
        delay(1);
    }
    return true;
}

int WsClient::Receive(uint32_t timeoutMs)
{
    if (_rx == nullptr)
        return -1;

    _rxLen               = 0;
    size_t total         = 0;
    const uint32_t deadline = millis() + timeoutMs;

    while (true)
    {
        if (_client.available() == 0)
        {
            if (!_client.connected())
                return -1;
            if (expired(deadline))
                return 0;
            delay(1);
            continue;
        }

        uint8_t head[2];
        if (!readExact(head, sizeof(head), deadline))
            return -1;

        const bool fin     = (head[0] & 0x80) != 0;
        const uint8_t op   = head[0] & 0x0F;
        const bool masked  = (head[1] & 0x80) != 0;
        uint64_t length    = head[1] & 0x7F;

        if (length == 126)
        {
            uint8_t ext[2];
            if (!readExact(ext, sizeof(ext), deadline))
                return -1;
            length = (static_cast<uint16_t>(ext[0]) << 8) | ext[1];
        }
        else if (length == 127)
        {
            uint8_t ext[8];
            if (!readExact(ext, sizeof(ext), deadline))
                return -1;
            length = 0;
            for (uint8_t byte : ext)
                length = (length << 8) | byte;
        }

        uint8_t mask[4] = {};
        if (masked && !readExact(mask, sizeof(mask), deadline))
            return -1;

        if (op == 0x8) // close
        {
            _client.stop();
            return -1;
        }
        if (op == 0x9 || op == 0xA) // ping / pong
        {
            if (length > sizeof(_chunk))
            {
                _error = "websocket control frame too large";
                return -1;
            }
            if (!readExact(_chunk, static_cast<size_t>(length), deadline))
                return -1;
            if (op == 0x9 && !sendFrame(0xA, _chunk, static_cast<size_t>(length)))
                return -1;
            continue;
        }
        if (op != 0x0 && op != 0x1 && op != 0x2)
        {
            uint8_t drain[64];
            while (length > 0)
            {
                const size_t count = length > sizeof(drain) ? sizeof(drain) : static_cast<size_t>(length);
                if (!readExact(drain, count, deadline))
                    return -1;
                length -= count;
            }
            continue;
        }

        if (total + length > _rxCap)
        {
            _error = "websocket frame too large";
            return -1;
        }

        const size_t offset = total;
        if (!readExact(_rx + offset, static_cast<size_t>(length), deadline))
            return -1;
        if (masked)
        {
            for (size_t i = 0; i < length; i++)
                _rx[offset + i] ^= mask[i & 3];
        }
        total += static_cast<size_t>(length);

        if (fin)
        {
            _rxLen = total;
            return static_cast<int>(total);
        }
    }
}
