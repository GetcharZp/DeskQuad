// time_service.h 封装网络时间
//
// 同步交给 ESP32 core 内置的 SNTP 客户端在后台完成，读取侧只做格式化，
// 不阻塞主循环。

#pragma once

#include <Arduino.h>
#include <time.h>

#include <deskquad/config/app_config.h>

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

    // Format 把时间格式化为 "H:MM"
    //
    // # Params:
    //
    //	out: 输出缓冲
    //	size: 缓冲长度，至少 8 字节
    void Format(char *out, size_t size) const;

private:
    const deskquad::Config &_config;
};
