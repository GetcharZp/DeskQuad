#include <deskquad/config/app_config.h>

namespace deskquad
{

    Config DefaultConfig()
    {
        Config cfg;
        cfg.apSsid         = "DeskQuad";
        cfg.apPassword     = "deskquad1234";
        cfg.ntpServer      = "ntp1.aliyun.com";
        cfg.ntpTimezone    = "CST-8";
        cfg.weatherApiBase = "http://api.seniverse.com/v3/weather/daily.json?key=";

        cfg.httpPort          = 80;
        cfg.voiceBaud         = 9600;
        cfg.adcDividerRatio   = 8.4f;
        cfg.batteryMinVoltage = 6.4f;
        cfg.batteryMaxVoltage = 8.4f;
        cfg.adcWindowSize     = 10;
        cfg.defaultCity       = "Shanghai";
        return cfg;
    }

} // namespace deskquad
