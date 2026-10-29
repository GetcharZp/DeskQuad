// response.h 统一 Web 接口的应答格式
//
// 命令类接口返回 {"status":"ok"}，错误统一为 {"status":"error","error":...}，
// 前端直接把 error 展示给用户。

#pragma once

#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>

namespace deskquad
{

    // SendJson 把 JsonDocument 序列化后发送
    inline void SendJson(AsyncWebServerRequest *request, int code, const JsonDocument &doc)
    {
        String body;
        serializeJson(doc, body);
        request->send(code, "application/json", body);
    }

    // SendError 发送一条错误应答
    inline void SendError(AsyncWebServerRequest *request, int code, const char *message)
    {
        JsonDocument doc;
        doc["status"] = "error";
        doc["error"]  = message;
        SendJson(request, code, doc);
    }

    // SendOk 发送成功应答
    inline void SendOk(AsyncWebServerRequest *request)
    {
        JsonDocument doc;
        doc["status"] = "ok";
        SendJson(request, 200, doc);
    }

} // namespace deskquad
