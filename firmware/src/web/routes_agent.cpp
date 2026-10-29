#include <deskquad/web/routes_agent.h>

#include <Arduino.h>
#include <ArduinoJson.h>
#include <WiFi.h>

#include <deskquad/web/response.h>

void RegisterAgentRoutes(AsyncWebServer &server, AppContext &context)
{
    // 聊天入口：提交消息后立即返回 request_id，结果由 /api/aiStatus 轮询
    server.on("/api/aitalk", HTTP_POST, [&context](AsyncWebServerRequest *request) {
        if (!request->hasParam("text", true))
        {
            deskquad::SendError(request, 400, "缺少 text 参数");
            return;
        }

        String text = request->getParam("text", true)->value();
        text.trim();
        if (text.length() == 0)
        {
            deskquad::SendError(request, 400, "消息内容为空");
            return;
        }
        if (!context.deepseek.isConfigured())
        {
            deskquad::SendError(request, 503, "AI 未配置，请先在设置页填写 API Key");
            return;
        }
        if (WiFi.status() != WL_CONNECTED)
        {
            deskquad::SendError(request, 503, "设备未连接 WiFi，请先在网络设置中联网");
            return;
        }

        const String requestId = context.agent.Submit(text);
        if (requestId.length() == 0)
        {
            deskquad::SendError(request, 409, "正在处理上一条消息，请稍候");
            return;
        }

        JsonDocument doc;
        doc["status"]     = "queued";
        doc["request_id"] = requestId;
        deskquad::SendJson(request, 202, doc);
    });

    // 状态查询：设备能力、当前网络与最近一次请求的结果一次返回
    server.on("/api/aiStatus", HTTP_GET, [&context](AsyncWebServerRequest *request) {
        const AgentSnapshot agent = context.agent.Snapshot();

        JsonDocument doc;
        doc["enabled"]    = context.deepseek.isConfigured();
        doc["wifi"]       = WiFi.status() == WL_CONNECTED;
        doc["busy"]       = agent.busy;
        doc["status"]     = agent.status;
        doc["request_id"] = agent.requestId;
        doc["reply"]      = agent.reply;
        doc["error"]      = agent.error;
        doc["model"]      = context.deepseek.getModel();
        doc["baseUrl"]    = context.deepseek.getBaseUrl();
        deskquad::SendJson(request, 200, doc);
    });

    server.on("/api/emotionStatus", HTTP_GET, [&context](AsyncWebServerRequest *request) {
        const PADState &pad = context.emotion.GetState();

        JsonDocument doc;
        doc["label"] = context.emotion.GetLabel();
        // 固定两位小数，serialized 让数值以数字而不是字符串输出
        doc["pleasure"]  = serialized(String(pad.pleasure, 2));
        doc["arousal"]   = serialized(String(pad.arousal, 2));
        doc["dominance"] = serialized(String(pad.dominance, 2));
        deskquad::SendJson(request, 200, doc);
    });

    // 读取已保存的 AI 凭据，供设置页回显；从未保存过时 configured 为 false
    server.on("/api/aiConfig", HTTP_GET, [&context](AsyncWebServerRequest *request) {
        deskquad::AiSettings settings;
        const bool configured = context.settings.LoadAi(settings);

        JsonDocument doc;
        doc["configured"] = configured;
        doc["apiKey"]     = settings.apiKey;
        doc["model"]      = settings.model;
        doc["baseUrl"]    = settings.baseUrl;
        deskquad::SendJson(request, 200, doc);
    });

    // 保存 AI 凭据到 NVS 并立即生效
    server.on("/api/aiConfig", HTTP_POST, [&context](AsyncWebServerRequest *request) {
        String apiKey  = request->hasParam("apiKey", true) ? request->getParam("apiKey", true)->value() : "";
        String model   = request->hasParam("model", true) ? request->getParam("model", true)->value() : "";
        String baseUrl = request->hasParam("baseUrl", true) ? request->getParam("baseUrl", true)->value() : "";
        apiKey.trim();
        model.trim();
        baseUrl.trim();

        if (apiKey.length() == 0)
        {
            deskquad::SendError(request, 400, "apiKey is required");
            return;
        }

        // 配置写入 NVS，刷新文件系统不会丢失
        deskquad::AiSettings settings;
        settings.apiKey  = apiKey;
        settings.model   = model;
        settings.baseUrl = baseUrl;
        if (!context.settings.SaveAi(settings))
        {
            deskquad::SendError(request, 500, "cannot save config to NVS");
            return;
        }
        Serial.println("[sys] agent: config saved");

        context.deepseek.configure(apiKey, model, baseUrl);

        JsonDocument doc;
        doc["status"] = "ok";
        deskquad::SendJson(request, 200, doc);
    });

    // 只清内存里的对话，凭据不动
    server.on("/api/aiClearHistory", HTTP_POST, [&context](AsyncWebServerRequest *request) {
        context.agent.ClearHistory();

        JsonDocument doc;
        doc["status"] = "ok";
        deskquad::SendJson(request, 200, doc);
    });
}
