#include <deskquad/web/routes_voice.h>

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

    // API Key 会直接拼进 WebSocket 请求头，拒绝换行等控制字符避免头注入
    bool hasControlChar(const String &value)
    {
        for (size_t i = 0; i < value.length(); i++)
        {
            const unsigned char c = static_cast<unsigned char>(value[i]);
            if (c < 0x20 || c == 0x7F)
                return true;
        }
        return false;
    }

    bool validApiKey(const String &value)
    {
        return value.length() > 0 && value.length() <= 127 && !hasControlChar(value);
    }

} // namespace

void RegisterVoiceRoutes(AsyncWebServer &server, AppContext &context)
{
    // 读取已保存的豆包配置，供设置页回显；从未保存过时 configured 为 false
    server.on("/api/voiceConfig", HTTP_GET, [&context](AsyncWebServerRequest *request) {
        deskquad::VoiceSettings settings;
        const bool configured = context.settings.LoadVoice(settings);

        JsonDocument doc;
        doc["configured"] = configured;
        doc["apiKey"]     = settings.apiKey;
        doc["active"]     = context.voice.HasApiKey();
        deskquad::SendJson(request, 200, doc);
    });

    // 保存豆包 API Key 到 NVS 并立即生效
    server.on("/api/voiceConfig", HTTP_POST, [&context](AsyncWebServerRequest *request) {
        if (!request->hasParam("apiKey", true))
        {
            deskquad::SendError(request, 400, "missing parameter");
            return;
        }

        String apiKey = paramOrEmpty(request, "apiKey");
        apiKey.trim();
        if (!validApiKey(apiKey))
        {
            deskquad::SendError(request, 400, "API Key 为空、过长或包含非法字符");
            return;
        }

        // 配置写入 NVS，刷新文件系统不会丢失
        deskquad::VoiceSettings settings;
        settings.apiKey = apiKey;
        if (!context.settings.SaveVoice(settings))
        {
            deskquad::SendError(request, 500, "cannot save config to NVS");
            return;
        }

        context.voice.SetApiKey(apiKey.c_str());
        Serial.println("[sys] voice: api key saved");

        JsonDocument doc;
        doc["status"]  = "ok";
        doc["message"] = "语音配置已保存";
        deskquad::SendJson(request, 200, doc);
    });

    // 测试接口：用表单里的 Key（不保存），未提供时回落到已保存的 Key。
    // 只做 TTS 握手，不合成音频，因此不会占用扬声器。
    server.on("/api/voice/test", HTTP_POST, [&context](AsyncWebServerRequest *request) {
        String apiKey = paramOrEmpty(request, "apiKey");
        apiKey.trim();
        if (apiKey.length() == 0)
        {
            deskquad::VoiceSettings settings;
            if (context.settings.LoadVoice(settings))
                apiKey = settings.apiKey;
        }

        if (!validApiKey(apiKey))
        {
            deskquad::SendError(request, 400, "请先填写正确的豆包 API Key");
            return;
        }
        if (WiFi.status() != WL_CONNECTED)
        {
            deskquad::SendError(request, 503, "设备未连接 WiFi，请先在网络设置中联网");
            return;
        }

        String message;
        if (!context.voice.TestApiKey(apiKey.c_str(), message))
        {
            deskquad::SendError(request, 502, message.c_str());
            return;
        }

        JsonDocument doc;
        doc["status"]  = "ok";
        doc["message"] = message;
        deskquad::SendJson(request, 200, doc);
    });
}
