// hlkv20.h HLK-V20 (SU-03T) 语音识别模块驱动 (UART)
//
// 接线:
//   HLK-V20 TX  → GPIO18 (UART1 RX)
//   HLK-V20 RX  → 可选 (仅用于 PC 配置工具)
//   HLK-V20 VCC → 3.3V
//   HLK-V20 GND → GND
//
// 模块每识别到一条命令就输出一行 ASCII，默认是 "CMD1".."CMD22"，
// 具体字符串与波特率在 HLK-V20 配置工具里设定。

#pragma once

#include <Arduino.h>
#include <HardwareSerial.h>
#include <deskquad/config/pins.h>
#include <deskquad/core/states.h>

// HLKV20 读取语音模块的串口输出
class HLKV20
{
public:
    // Begin 在 rxPin 上启动只收不发的 UART
    //
    // # Params:
    //
    //	rxPin: 模块 TX 所接的引脚
    //	baud: 模块波特率，需与配置工具一致
    void Begin(uint8_t rxPin = deskquad::kVoiceRxPin, uint32_t baud = 9600);

    // Poll 读完串口缓冲，返回最后一条完整命令
    //
    // # Returns:
    //
    //	VoiceCommand::None 表示这一轮没有识别结果
    VoiceCommand Poll();

    // IsAvailable 返回 UART 是否已启动
    bool IsAvailable() const { return _initialized; }

private:
    HardwareSerial _serial{1};
    char _lineBuf[16] = {}; // 一行 ASCII，最长 "CMD22"
    uint8_t _linePos  = 0;
    bool _initialized = false;
};
