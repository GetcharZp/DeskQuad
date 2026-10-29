#include <deskquad/web/routes_network.h>

#include <Arduino.h>
#include <ArduinoJson.h>
#include <WiFi.h>

#include <deskquad/web/response.h>

namespace
{

    String paramOrEmpty(AsyncWebServerRequest *request, const char *name)
    {
        return request->hasParam(name, true) ? request->getParam(name, true)->value() : String();
    }

} // namespace

void RegisterNetworkRoute(AsyncWebServer &server, AppContext &context)
{
    // 当前网络状态：页面据此展示已连接的 WiFi 与可访问的地址
    server.on("/api/networkStatus", HTTP_GET, [](AsyncWebServerRequest *request) {
        const bool connected = WiFi.status() == WL_CONNECTED;

        JsonDocument doc;
        doc["connected"] = connected;
        doc["ssid"]      = connected ? WiFi.SSID() : "";
        doc["ip"]        = connected ? WiFi.localIP().toString() : "";
        doc["apIp"]      = WiFi.softAPIP().toString();
        doc["rssi"]      = connected ? WiFi.RSSI() : 0;
        deskquad::SendJson(request, 200, doc);
    });

    // 读取已保存的 WiFi 配置，供设置页回显；从未保存过时 configured 为 false
    server.on("/api/wifiConfig", HTTP_GET, [&context](AsyncWebServerRequest *request) {
        deskquad::WifiSettings settings;
        const bool configured = context.settings.LoadWifi(settings);

        JsonDocument doc;
        doc["configured"] = configured;
        doc["ssid"]       = settings.ssid;
        doc["pass"]       = settings.pass;
        deskquad::SendJson(request, 200, doc);
    });

    // WiFi 配置单独保存，天气配置由 routes_weather 处理
    server.on("/api/wifiConfig", HTTP_POST, [&context](AsyncWebServerRequest *request) {
        if (!request->hasParam("ssid", true) || !request->hasParam("pass", true))
        {
            deskquad::SendError(request, 400, "missing parameter");
            return;
        }

        const String ssid = paramOrEmpty(request, "ssid");
        const String pass = paramOrEmpty(request, "pass");
        if (ssid.length() == 0)
        {
            deskquad::SendError(request, 400, "ssid is required");
            return;
        }

        // 配置写入 NVS，刷新文件系统不会丢失
        deskquad::WifiSettings settings;
        settings.ssid = ssid;
        settings.pass = pass;
        if (!context.settings.SaveWifi(settings))
        {
            deskquad::SendError(request, 500, "cannot save config to NVS");
            return;
        }

        Serial.printf("[sys] wifi: connecting to %s\n", ssid.c_str());
        WiFi.begin(ssid.c_str(), pass.c_str());
        deskquad::SendOk(request);
    });
}
