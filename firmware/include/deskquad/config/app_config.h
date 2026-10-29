// app_config.h 定义 DeskQuad 的运行参数
//
// GPIO 分配不在这里，见 config/pins.h。

#pragma once

#include <stdint.h>

namespace deskquad
{

    // Config 定义 DeskQuad 的运行参数
    struct Config
    {
        // 必填参数
        const char *apSsid;         // 配网热点名称
        const char *apPassword;     // 配网热点密码，WPA2 要求至少 8 位
        const char *ntpServer;      // NTP 服务器
        const char *ntpTimezone;    // POSIX 时区字符串，如 "CST-8"、"CET-1CEST,M3.5.0,M10.5.0/3"
        const char *weatherApiBase; // 心知天气 API 前缀，密钥由网页配置追加

        // 可选参数
        uint16_t httpPort;       // (可选) HTTP 服务端口，默认 80
        uint32_t voiceBaud;      // (可选) HLK-V20 波特率，默认 9600
        float adcDividerRatio;   // (可选) 电池分压比，默认 8.4
        float batteryMinVoltage; // (可选) 电量 0% 对应电压，默认 6.4
        float batteryMaxVoltage; // (可选) 电量 100% 对应电压，默认 8.4
        uint8_t adcWindowSize;   // (可选) ADC 滑动平均窗口，默认 10
        const char *defaultCity; // (可选) 默认天气城市，默认 Shanghai
    };

    // DefaultConfig 返回出厂配置
    Config DefaultConfig();

} // namespace deskquad
