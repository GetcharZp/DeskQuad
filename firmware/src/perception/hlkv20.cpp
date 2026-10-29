#include <deskquad/perception/hlkv20.h>

#include <stdlib.h>
#include <string.h>

namespace
{

    constexpr long kMaxCommand = static_cast<long>(VoiceCommand::Error);

    // parseCommand 把一行 ASCII 解析成命令编号
    //
    // 兼容配置工具默认输出的 "CMD12" 和直接输出数字的 "12"；其余内容一律
    // 当作无效行忽略，避免串口噪声触发动作。
    VoiceCommand parseCommand(const char *line)
    {
        while (*line == ' ')
            ++line;
        if (strncmp(line, "CMD", 3) == 0)
            line += 3;

        char *end     = nullptr;
        const long id = strtol(line, &end, 10);
        while (*end == ' ')
            ++end;
        if (end == line || *end != '\0' || id < 1 || id > kMaxCommand)
            return VoiceCommand::None;

        return static_cast<VoiceCommand>(id);
    }

} // namespace

void HLKV20::Begin(uint8_t rxPin, uint32_t baud)
{
    if (_initialized)
        return;

    // TX 传 -1：模块只输出，不需要发送
    _serial.begin(baud, SERIAL_8N1, rxPin, -1);
    _linePos     = 0;
    _initialized = true;
}

VoiceCommand HLKV20::Poll()
{
    if (!_initialized)
        return VoiceCommand::None;

    VoiceCommand latest = VoiceCommand::None;
    while (_serial.available() > 0)
    {
        const char c = static_cast<char>(_serial.read());
        if (c != '\n' && c != '\r')
        {
            if (c >= 32 && c < 127 && _linePos < sizeof(_lineBuf) - 1)
                _lineBuf[_linePos++] = c;
            continue;
        }

        if (_linePos == 0)
            continue;

        _lineBuf[_linePos] = '\0';
        _linePos           = 0;

        const VoiceCommand cmd = parseCommand(_lineBuf);
        if (cmd != VoiceCommand::None)
            latest = cmd;
    }
    return latest;
}
