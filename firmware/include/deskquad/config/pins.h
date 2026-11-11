// pins.h 声明 DeskQuad 的全部 GPIO 分配，是引脚定义的唯一真源
//
// 目标板为 ESP32-S3-N16R8，选择引脚时避开了：
//   GPIO0/3/45/46  strapping；GPIO46 仅作为摄像头数据输入，输入脚不影响启动
//   GPIO26-32      片内 Flash
//   GPIO33-37      Octal PSRAM
//   GPIO39-42      JTAG（39-41 已用于功放、42 已用于摄像头 D6，JTAG 不可用）
//   GPIO43/44      UART0
//   GPIO48         开发板板载 RGB LED
//
// 摄像头使用 DVP 并口，GPIO19/20（原生 USB）被数据线占用，
// 烧录与串口请使用板载 UART 口，不要连接原生 USB 口。

#pragma once

#include <stdint.h>

namespace deskquad
{

    // 四路舵机 PWM 引脚，顺序为 Servo1..4
    inline constexpr uint8_t kServoPins[4] = {14, 16, 12, 13};

    // OLED I2C 引脚（SSD1306 128x64，U8g2 软件 I2C）
    inline constexpr uint8_t kOledClockPin = 5;
    inline constexpr uint8_t kOledDataPin  = 4;

    // I2S 麦克风 (INMP441)：SCK / WS / SD，L/R 接 GND 选择左声道
    inline constexpr uint8_t kMicI2sBclkPin = 11;
    inline constexpr uint8_t kMicI2sWsPin   = 47;
    inline constexpr uint8_t kMicI2sDinPin  = 18;

    // I2S 功放 (MAX98357A)：BCLK / LRC / DIN，VIN 接 5V
    // 39-41 属于 JTAG，占用后调试器不可用，USB 串口仍然可用。
    // TTS 与旋律提示音都由该功放输出。
    inline constexpr uint8_t kAmpI2sBclkPin = 39;
    inline constexpr uint8_t kAmpI2sLrcPin  = 40;
    inline constexpr uint8_t kAmpI2sDinPin  = 41;

    // OV2640 DVP 摄像头（8 位并口 + SCCB）
    //
    // 模块接线：
    //   3.3V -> 3.3V        GND   -> GND
    //   SCL  -> GPIO2       SDA   -> GPIO15
    //   VSYNC-> GPIO17      HREF  -> GPIO6       PCLK -> GPIO7
    //   XCLK -> GPIO21
    //   D0..D7 -> GPIO8, GPIO9, GPIO10, GPIO19, GPIO20, GPIO38, GPIO42, GPIO46
    //   PWDN -> GND         RESET -> 3.3V
    //
    // PWDN/RESET 不占用 GPIO：模块上通常已有下拉/上拉，若没有请按上面的
    // 电平手动接线后再上电。GPIO46 是输入专用脚，正好用于摄像头数据输入。
    // XCLK 放在 GPIO21，远离舵机引脚，减少 20MHz 时钟对舵机 PWM 的串扰。
    //
    // 若板子只有原生 USB 口（GPIO19/20）可烧录，请改用：
    //   kCameraPinD3 = 3, kCameraPinD4 = 48
    // 同时保持 GPIO19/20 空闲。GPIO48 是板载 RGB LED，会随数据闪烁。
    inline constexpr int kCameraPinPwdn  = -1;
    inline constexpr int kCameraPinReset = -1;
    inline constexpr int kCameraPinXclk  = 21;
    inline constexpr int kCameraPinSiod  = 15; // SCCB SDA
    inline constexpr int kCameraPinSioc  = 2;  // SCCB SCL
    inline constexpr int kCameraPinVsync = 17;
    inline constexpr int kCameraPinHref  = 6;
    inline constexpr int kCameraPinPclk  = 7;
    inline constexpr int kCameraPinD0    = 8;
    inline constexpr int kCameraPinD1    = 9;
    inline constexpr int kCameraPinD2    = 10;
    inline constexpr int kCameraPinD3    = 19;
    inline constexpr int kCameraPinD4    = 20;
    inline constexpr int kCameraPinD5    = 38;
    inline constexpr int kCameraPinD6    = 42;
    inline constexpr int kCameraPinD7    = 46;

    // 电池分压采样引脚（ADC1_CH0，ADC2 在 WiFi 开启时不可用）
    inline constexpr uint8_t kAdcPin = 1;

} // namespace deskquad
