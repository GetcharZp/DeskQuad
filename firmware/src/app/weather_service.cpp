#include <deskquad/app/weather_service.h>

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClient.h>

namespace
{
    // 查询失败后的重试间隔，避免天气页停留时反复发起阻塞请求
    constexpr unsigned long kRetryIntervalMs = 60000;
} // namespace

WeatherService::WeatherService(const deskquad::Config &config) : _config(config)
{
    _city       = config.defaultCity;
    _data.ready = false;
}

void WeatherService::Reset()
{
    _data.ready = false;
    _attempted  = false; // 城市或密钥变化后立即重新查询
}

void WeatherService::Update(bool wifiConnected)
{
    if (_data.ready || !wifiConnected || _apiKey.length() == 0)
        return;
    if (_attempted && millis() - _lastAttemptMs < kRetryIntervalMs)
        return;

    _attempted     = true;
    _lastAttemptMs = millis();

    WiFiClient client;
    HTTPClient http;

    String url =
        String(_config.weatherApiBase) + _apiKey + "&location=" + _city + "&language=zh-Hans&unit=c&start=0&days=1";

    if (!http.begin(client, url))
    {
        Serial.println("[warn] weather: cannot reach API host");
        return;
    }

    const int httpCode = http.GET();
    if (httpCode > 0)
    {
        const String payload = http.getString();
        JsonDocument doc;
        const DeserializationError error = deserializeJson(doc, payload);
        if (!error)
        {
            _data.temperature = doc["results"][0]["daily"][0]["high"].as<String>();
            _data.humidity    = doc["results"][0]["daily"][0]["humidity"].as<String>();
            _data.weather     = doc["results"][0]["daily"][0]["text_day"].as<String>();
            _data.ready       = true;
            Serial.printf("[sys] weather: %s %s%% %s\n", _data.temperature.c_str(), _data.humidity.c_str(),
                          _data.weather.c_str());
        }
        else
        {
            Serial.printf("[warn] weather: JSON parse failed: %s\n", error.c_str());
        }
    }
    else
    {
        Serial.printf("[warn] weather: HTTP %d %s\n", httpCode, http.errorToString(httpCode).c_str());
    }
    http.end();
}
