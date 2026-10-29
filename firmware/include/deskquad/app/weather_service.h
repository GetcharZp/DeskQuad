// weather_service.h 获取并解析天气数据
//
// 只负责取数，画屏交给 weather_view；两者拆开之后天气逻辑可以在
// 没有屏幕的情况下单独检查。

#pragma once

#include <Arduino.h>

#include <deskquad/config/app_config.h>
#include <deskquad/render/weather_data.h>

// WeatherService 通过心知天气 API 查询当天天气
class WeatherService
{
public:
    // 构造函数
    //
    // # Params:
    //
    //	config: 运行配置，提供 API 前缀与默认城市
    explicit WeatherService(const deskquad::Config &config);

    // Update 在联网且尚未取到数据时发起一次查询
    //
    // 结果会缓存，重复调用不会重复请求。
    void Update(bool wifiConnected);

    // Reset 丢弃缓存，下次 Update 会重新查询
    void Reset();

    const WeatherData &Data() const { return _data; }

    // SetCity 设置查询城市
    void SetCity(const String &city) { _city = city; }

    // SetApiKey 设置心知天气密钥
    void SetApiKey(const String &key) { _apiKey = key; }

    const String &City() const { return _city; }
    const String &ApiKey() const { return _apiKey; }

private:
    const deskquad::Config &_config;
    String _apiKey;
    String _city;
    WeatherData _data;

    // 失败重试的节流状态：没有它，天气页会按主循环频率反复发起阻塞请求
    bool _attempted              = false;
    unsigned long _lastAttemptMs = 0;
};
