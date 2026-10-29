// DeepSeek AI Agent — HTTP 客户端
// 通过 HTTPS 调用 DeepSeek 开放平台 推理 API
#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <deskquad/agent/agent_config.h>
#include <deskquad/agent/agent_types.h>

// DeepSeekAgent 只负责一次 OpenAI 兼容的 chat/completions 调用：
// 组装请求、发送、解析成 AgentResponse。
// 情感上下文由调用方拼进消息，失败后的设备反馈也由调用方决定。
class DeepSeekAgent
{
public:
    // ---------- 配置 ----------
    void configure(const String &apiKey, const String &model, const String &baseUrl = "");
    bool isConfigured() const { return _apiKey.length() > 0; }
    const String &getBaseUrl() const { return _baseUrl; }
    const String &getModel() const { return _model; }

    // ---------- 核心 API 调用 ----------
    // 阻塞直到请求结束，只能在 loop 任务中调用。
    // prompt 是已经拼好情感上下文的完整用户消息；historyText 是用户原话，只用于记录历史。
    // 返回 false 时通过 getLastError() 取原因，response 保持不变。
    bool ask(const String &prompt, const String &historyText, AgentResponse &response);

    // ---------- 最近一次失败原因 ----------
    const String &getLastError() const { return _lastError; }

    // ---------- 对话管理 ----------
    void clearHistory();

private:
    bool buildRequestBody(const String &prompt);
    bool postRequest(String &body);
    bool parseApiResponse(const char *json, AgentResponse &response);
    void appendHistory(const String &user, const String &assistant);

    String _apiKey;
    String _model;
    String _baseUrl;
    String _lastError; // 最近一次失败原因，供 Web 端展示

    // 对话历史，按时间顺序保存最近 AGENT_MAX_HISTORY 轮
    ConversationEntry _history[AGENT_MAX_HISTORY];
    uint8_t _historyCount = 0;

    // 请求体复用静态缓冲区，避免反复分配堆内存
    char _requestBuf[AGENT_REQ_BUF_SIZE] = {};

    // 复用 HTTP 客户端；直连 DeepSeek 走 TLS，局域网网关通常是明文 HTTP
    WiFiClientSecure _secureClient;
    WiFiClient _plainClient;
    HTTPClient _http;
};
