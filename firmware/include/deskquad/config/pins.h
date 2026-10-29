// pins.h 声明 DeskQuad 的全部 GPIO 分配，是引脚定义的唯一真源
//
// 目标板为 ESP32-S3-N16R8，选择引脚时避开了：
//   GPIO0/3/45/46  strapping
//   GPIO19/20      USB D-/D+
//   GPIO26-32      片内 Flash
//   GPIO33-37      Octal PSRAM
//   GPIO39-42      JTAG
//   GPIO43/44      UART0

#pragma once

#include <stdint.h>

namespace deskquad
{

    // 四路舵机 PWM 引脚，顺序为 Servo1..4
    inline constexpr uint8_t kServoPins[4] = {14, 16, 12, 13};

    // OLED I2C 引脚（SSD1306 128x64，U8g2 软件 I2C）
    inline constexpr uint8_t kOledClockPin = 5;
    inline constexpr uint8_t kOledDataPin  = 4;

    // 按键引脚，顺序为 Button1..2
    inline constexpr uint8_t kButtonPins[2] = {2, 15};

    // 扬声器引脚
    inline constexpr uint8_t kSpeakerPin = 21;

    // HLK-V20 语音模块 TX 接此引脚（UART1 RX）
    inline constexpr uint8_t kVoiceRxPin = 18;

    // OV2640 摄像头：SPI 数据线与 SCCB 配置线
    inline constexpr uint8_t kCameraCsPin   = 17;
    inline constexpr uint8_t kCameraSckPin  = 6;
    inline constexpr uint8_t kCameraMisoPin = 7;
    inline constexpr uint8_t kCameraMosiPin = 8;
    inline constexpr uint8_t kCameraSdaPin  = 9;
    inline constexpr uint8_t kCameraSclPin  = 10;

    // 电池分压采样引脚（ADC1_CH0，ADC2 在 WiFi 开启时不可用）
    inline constexpr uint8_t kAdcPin = 1;

} // namespace deskquad
