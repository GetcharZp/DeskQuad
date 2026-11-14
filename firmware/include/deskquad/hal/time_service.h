// time_service.h 封装网络时间
//
// 同步交给 ESP32 core 内置的 SNTP 客户端在后台完成，读取侧只做格式化，
// 不阻塞主循环。

#pragma once

#include <Arduino.h>
#include <time.h>

#include <deskquad/config/app_config.h>

// ClockReading 是一次时间读取的快照
//
// 显示层只需要这一组字段：时间页把 hour/minute 画成大号数字，
// second 驱动秒进度与冒号闪烁，month/day/weekday 组成日期行。
struct ClockReading
{
    uint8_t hour;    // 0..23
    uint8_t minute;  // 0..59
    uint8_t second;  // 0..59
    uint8_t month;   // 1..12
    uint8_t day;     // 1..31
    uint8_t weekday; // 0 = 周日
    bool synced;     // 是否已经从 NTP 取得有效时间
};

// TimeService 提供网络时间
class TimeService
{
public:
    // 构造函数
    //
    // # Params:
    //
    //	config: 运行配置，提供 NTP 服务器与时区
    explicit TimeService(const deskquad::Config &config);

    // Begin 启动 SNTP 同步，需要在联网之后调用
    void Begin();

    // Now 读取当前本地时间
    ClockReading Now() const;

private:
    const deskquad::Config &_config;
};
