#include <deskquad/web/routes_weather.h>

#include <Arduino.h>
#include <ArduinoJson.h>

#include <deskquad/web/response.h>

void RegisterWeatherRoutes(AsyncWebServer &server, AppContext &context)
{
    // 读取已保存的天气配置，供设置页回显；从未保存过时 configured 为 false
    server.on("/api/weatherConfig", HTTP_GET, [&context](AsyncWebServerRequest *request) {
        deskquad::WeatherSettings settings;
        const bool configured = context.settings.LoadWeather(settings);

        JsonDocument doc;
        doc["configured"] = configured;
        doc["city"]       = settings.city;
        doc["apiKey"]     = settings.apiKey;
        deskquad::SendJson(request, 200, doc);
    });

    // 天气配置单独保存，WiFi 配置由 routes_network 处理
    server.on("/api/weatherConfig", HTTP_POST, [&context](AsyncWebServerRequest *request) {
        if (!request->hasParam("city", true) || !request->hasParam("api", true))
        {
            deskquad::SendError(request, 400, "missing parameter");
            return;
        }

        String city = request->getParam("city", true)->value();
        String api  = request->getParam("api", true)->value();
        city.trim();
        api.trim();
        if (city.length() == 0 || api.length() == 0)
        {
            deskquad::SendError(request, 400, "city and api are required");
            return;
        }

        // 配置写入 NVS，刷新文件系统不会丢失
        deskquad::WeatherSettings settings;
        settings.city   = city;
        settings.apiKey = api;
        if (!context.settings.SaveWeather(settings))
        {
            deskquad::SendError(request, 500, "cannot save config to NVS");
            return;
        }

        context.weather.SetCity(city);
        context.weather.SetApiKey(api);
        context.weather.Reset(); // 丢弃旧城市/密钥的缓存，下次进入天气页立即重新查询
        Serial.printf("[sys] weather: configured for %s\n", city.c_str());
        deskquad::SendOk(request);
    });
}
