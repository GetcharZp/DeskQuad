#include <deskquad/hal/time_service.h>

TimeService::TimeService(const deskquad::Config &config) : _config(config) {}

void TimeService::Begin()
{
    // ESP32 的 configTime 参数是 GMT 偏移，POSIX 时区串要走 configTzTime
    configTzTime(_config.ntpTimezone, _config.ntpServer);
}

ClockReading TimeService::Now() const
{
    struct tm now  = {};
    const time_t t = time(nullptr);
    localtime_r(&t, &now);

    ClockReading reading;
    reading.hour    = static_cast<uint8_t>(now.tm_hour);
    reading.minute  = static_cast<uint8_t>(now.tm_min);
    reading.second  = static_cast<uint8_t>(now.tm_sec);
    reading.month   = static_cast<uint8_t>(now.tm_mon + 1);
    reading.day     = static_cast<uint8_t>(now.tm_mday);
    reading.weekday = static_cast<uint8_t>(now.tm_wday);
    // SNTP 同步前 time() 返回 1970 年，用年份判断是否已有可信时间
    reading.synced = (now.tm_year + 1900) >= 2024;
    return reading;
}
