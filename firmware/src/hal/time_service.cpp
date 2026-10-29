#include <deskquad/hal/time_service.h>

#include <stdio.h>

namespace
{

    // localTime 取当前本地时间，时区由 configTzTime 设置的 TZ 决定
    struct tm localTime()
    {
        struct tm now  = {};
        const time_t t = time(nullptr);
        localtime_r(&t, &now);
        return now;
    }

} // namespace

TimeService::TimeService(const deskquad::Config &config) : _config(config) {}

void TimeService::Begin()
{
    // ESP32 的 configTime 参数是 GMT 偏移，POSIX 时区串要走 configTzTime
    configTzTime(_config.ntpTimezone, _config.ntpServer);
}

void TimeService::Format(char *out, size_t size) const
{
    const struct tm now = localTime();
    snprintf(out, size, "%d:%02d", now.tm_hour, now.tm_min);
}
