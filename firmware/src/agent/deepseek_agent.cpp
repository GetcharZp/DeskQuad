// DeepSeek AI Agent — HTTP 客户端实现
// 调用 DeepSeek 开放平台 OpenAI 兼容接口
#include <deskquad/agent/agent_content.h>
#include <deskquad/agent/deepseek_agent.h>
#include <string.h>

namespace
{

    // extractErrorMessage 从上游错误体里取人类可读的原因
    // DeepSeek 返回 {"error":{"message":"...","type":"..."}}
    String extractErrorMessage(const String &body)
    {
        JsonDocument doc;
        if (deserializeJson(doc, body))
            return String();

        const char *message = doc["error"]["message"] | "";
        if (message[0] == '\0')
            message = doc["message"] | "";
        return String(message);
    }

    // resolveEndpoint 兼容三种填法：
    //   https://api.deepseek.com                -> 补全 /chat/completions
    //   https://api.deepseek.com/v1             -> 补全 /chat/completions
    //   https://host/v1/chat/completions        -> 原样使用
    String resolveEndpoint(String url)
    {
        url.trim();
        while (url.endsWith("/"))
        {
            url.remove(url.length() - 1);
        }

        if (url.length() == 0)
        {
            return String(DEEPSEEK_BASE_URL) + DEEPSEEK_API_PATH;
        }

        if (url.indexOf(DEEPSEEK_API_PATH) < 0)
        {
            url += DEEPSEEK_API_PATH;
        }
        return url;
    }

} // namespace

// 配置
void DeepSeekAgent::configure(const String &apiKey, const String &model, const String &baseUrl)
{
    _apiKey = apiKey;
    _apiKey.trim();

    _model = model;
    _model.trim();
    if (_model.length() == 0)
    {
        _model = DEEPSEEK_DEFAULT_MODEL;
    }

    _baseUrl   = resolveEndpoint(baseUrl);
    _lastError = "";

    Serial.printf("[sys] agent: endpoint %s\n", _baseUrl.c_str());
    Serial.println(isConfigured() ? "[sys] agent: configured" : "[sys] agent: no api key, disabled");
}

// 构建请求体
bool DeepSeekAgent::buildRequestBody(const String &prompt, bool strict)
{
    // JsonDocument 按需在堆上增长，不必预估系统提示词与历史消息的体积
    JsonDocument doc;

    // --- messages 数组 ---
    JsonArray messages = doc["messages"].to<JsonArray>();

    // 1. System prompt
    JsonObject sysMsg = messages.add<JsonObject>();
    sysMsg["role"]    = "system";
    sysMsg["content"] = AGENT_SYSTEM_PROMPT;

    // 2. 对话历史
    for (uint8_t i = 0; i < _historyCount; i++)
    {
        JsonObject histUser = messages.add<JsonObject>();
        histUser["role"]    = "user";
        histUser["content"] = _history[i].user;

        JsonObject histAsst = messages.add<JsonObject>();
        histAsst["role"]    = "assistant";
        histAsst["content"] = _history[i].assistant;
    }

    // 3. 当前用户输入 (含情感上下文)
    // 格式重试时在末尾追加一条更硬的约束：模型偶尔会无视 response_format，
    // 把一句普通文本或空白当成回复。
    String userContent = prompt;
    if (strict)
    {
        userContent += "\n\n（上一次回复不是合法 json。本次只输出一个 json 对象，不要解释、不要 Markdown。）";
    }

    JsonObject curMsg = messages.add<JsonObject>();
    curMsg["role"]    = "user";
    curMsg["content"] = userContent;

    // --- model ---
    doc["model"] = _model;

    // --- thinking ---
    // deepseek-flash 默认开启思考模式，推理与正文共享 max_tokens 预算；
    // 预算被推理耗尽时会返回空 content。这里只需要一句 JSON 回复，
    // 关闭思考既保证内容完整，也把首字延迟从数秒压到亚秒级。
    doc["thinking"]["type"] = "disabled";

    // --- response_format ---
    // DeepSeek 要求开启 json_object 时提示词中出现 "json"，系统提示词已满足
    doc["response_format"]["type"] = "json_object";

    // --- temperature / max_tokens ---
    doc["temperature"] = strict ? 0.2f : 0.7f;
    doc["max_tokens"]  = strict ? AGENT_RETRY_MAX_TOKENS : AGENT_MAX_TOKENS;

    // 先量体积再序列化，避免把截断后的半个 JSON 发给上游
    const size_t need = measureJson(doc);
    if (need >= AGENT_REQ_BUF_SIZE)
    {
        Serial.printf("[sys] agent: request too large (%u >= %u bytes)\n", static_cast<unsigned>(need),
                      static_cast<unsigned>(AGENT_REQ_BUF_SIZE));
        _lastError = "请求体过大，无法发送";
        return false;
    }

    const size_t len = serializeJson(doc, _requestBuf, AGENT_REQ_BUF_SIZE);
    Serial.printf("[sys] agent: request built (%u bytes)\n", static_cast<unsigned>(len));
    return true;
}

// 发送 HTTP 请求，成功时把响应体写入 body
bool DeepSeekAgent::postRequest(String &body)
{
    int httpCode = -1;

    for (int attempt = 0; attempt <= AGENT_MAX_RETRIES; attempt++)
    {
        if (attempt > 0)
            delay(AGENT_RETRY_DELAY);

        // 每次尝试都重新握手，避免复用上一次失败留下的连接状态
        const bool secure  = _baseUrl.startsWith("https://");
        WiFiClient &client = secure ? static_cast<WiFiClient &>(_secureClient) : _plainClient;
        client.stop();
        if (secure)
            _secureClient.setInsecure(); // Demo 阶段关闭证书校验；生产环境应改用 setCACert 加载 CA

        if (!_http.begin(client, _baseUrl))
        {
            _lastError = "HTTP 客户端初始化失败";
            continue;
        }

        _http.setConnectTimeout(AGENT_HTTP_TIMEOUT);
        _http.setTimeout(AGENT_HTTP_TIMEOUT);
        _http.setReuse(false);
        _http.addHeader("Content-Type", "application/json");
        _http.addHeader("Authorization", "Bearer " + _apiKey);

        httpCode = _http.POST(reinterpret_cast<uint8_t *>(_requestBuf), strlen(_requestBuf));
        body     = _http.getString();
        _http.end();

        if (httpCode == 200)
            return true;

        if (httpCode > 0)
        {
            const String upstream = extractErrorMessage(body);
            _lastError            = "HTTP " + String(httpCode);
            if (upstream.length() > 0)
                _lastError += "：" + upstream;
            Serial.printf("[sys] agent: HTTP %d %s\n", httpCode, upstream.c_str());

            // 4xx 是密钥、模型或请求格式的问题，重试不会改变结果
            if (httpCode < 500)
                break;
        }
        else
        {
            const String reason = HTTPClient::errorToString(httpCode);
            _lastError          = "网络错误：" + reason;
            Serial.printf("[sys] agent: network error %d (%s)\n", httpCode, reason.c_str());
        }
    }

    if (_lastError.length() == 0)
        _lastError = "网络请求失败";
    return false;
}

// 解析 API 响应
bool DeepSeekAgent::parseApiResponse(const char *json, AgentResponse &response)
{
    JsonDocument apiDoc;
    if (deserializeJson(apiDoc, json))
    {
        Serial.println("[sys] agent: response is not valid json");
        _lastError = "AI 响应不是合法 JSON";
        return false;
    }

    const char *content        = apiDoc["choices"][0]["message"]["content"] | "";
    const char *finishReason   = apiDoc["choices"][0]["finish_reason"] | "unknown";
    const char *reasoning      = apiDoc["choices"][0]["message"]["reasoning_content"] | "";
    const int completionTokens = apiDoc["usage"]["completion_tokens"] | -1;

    // 上游偶尔只返回一个空格或换行；统一先去掉首尾空白再判断
    String text(content);
    text.trim();

    // 模型偶尔会把 JSON 包在 Markdown 代码块里，截取最外层 {...}
    const char *start = strchr(text.c_str(), '{');
    const char *end   = strrchr(text.c_str(), '}');
    if (start == nullptr || end == nullptr || end <= start)
    {
        // 模型无视 JSON 约束、直接说了一句人话：按纯文本回复处理。
        // 只说人话时对话还能继续，比整轮失败、播一句错误提示自然得多。
        const bool looksLikeJson = text.startsWith("{") || text.startsWith("[") || text.startsWith("```");
        if (text.length() > 0 && !looksLikeJson)
        {
            Serial.printf("[warn] agent: non-json reply, used as plain text: %s\n", text.c_str());
            response.reply_text = text.substring(0, AGENT_MAX_REPLY_LEN);
            response.action     = 0;
            response.expression = -1;
            response.melody     = -1;
            response.repeat     = 1;
            response.emotion    = 0;
            return true;
        }

        // 空白或截断的 JSON：记下上游线索，交给 ask() 决定是否重试。
        // reasoning 字节数不为 0 说明思考模式并没有真正关闭；
        // completion_tokens 顶到上限说明正文预算被耗尽。
        Serial.printf("[warn] agent: unusable content (finish_reason=%s, completion_tokens=%d, content=%u B, "
                      "reasoning=%u B)\n",
                      finishReason, completionTokens, static_cast<unsigned>(text.length()),
                      static_cast<unsigned>(strlen(reasoning)));
        _lastError = String("AI 回复缺少 JSON 对象 (finish_reason=") + finishReason + ")";
        return false;
    }

    JsonDocument contentDoc;
    if (deserializeJson(contentDoc, start, static_cast<size_t>(end - start + 1)))
    {
        Serial.println("[sys] agent: content json parse error");
        _lastError = "AI 回复的 JSON 无法解析";
        return false;
    }

    response.reply_text = contentDoc["reply_text"] | "";
    if (response.reply_text.length() == 0)
    {
        _lastError = "AI 回复缺少 reply_text";
        return false;
    }

    response.action     = contentDoc["action"] | -1;
    response.expression = contentDoc["expression"] | -1;
    response.melody     = contentDoc["melody"] | -1;
    response.repeat     = contentDoc["repeat"] | 0;
    response.emotion    = contentDoc["emotion"] | 0;

    Serial.printf("[sys] agent: parsed reply=\"%s\" action=%d expr=%d melody=%d repeat=%d emotion=%d\n",
                  response.reply_text.c_str(), response.action, response.expression, response.melody, response.repeat,
                  response.emotion);
    return true;
}

// 追加对话历史
void DeepSeekAgent::appendHistory(const String &user, const String &assistant)
{
    // 写满后整体前移一格，始终按时间顺序保存最近 AGENT_MAX_HISTORY 轮
    if (_historyCount == AGENT_MAX_HISTORY)
    {
        for (uint8_t i = 1; i < AGENT_MAX_HISTORY; i++)
        {
            _history[i - 1] = _history[i];
        }
        _historyCount--;
    }

    _history[_historyCount].user      = user.substring(0, AGENT_MAX_USER_LEN);
    _history[_historyCount].assistant = assistant.substring(0, AGENT_MAX_REPLY_LEN);
    _historyCount++;
}

// 清除历史
void DeepSeekAgent::clearHistory()
{
    for (auto &entry : _history)
    {
        entry.user      = "";
        entry.assistant = "";
    }
    _historyCount = 0;
    Serial.println("[sys] agent: history cleared");
}

// 核心 API 调用
bool DeepSeekAgent::ask(const String &prompt, const String &historyText, AgentResponse &response)
{
    _lastError = "";

    if (!isConfigured())
    {
        _lastError = "AI 未配置，请先在设置页填写 API Key";
        return false;
    }

    if (WiFi.status() != WL_CONNECTED)
    {
        Serial.println("[sys] agent: wifi not connected");
        _lastError = "设备未连接 WiFi，请先在网络设置中联网";
        return false;
    }

    // 上游偶尔返回空白内容或纯文本。解析失败时换一组更保守的参数整轮重问一次：
    // 温度更低、预算更宽，并追加一条更硬的 JSON 约束。
    for (int attempt = 0; attempt <= AGENT_PARSE_RETRIES; attempt++)
    {
        const bool strict = attempt > 0;

        // 失败时 buildRequestBody 自己写入 _lastError
        if (!buildRequestBody(prompt, strict))
            return false;

        String body;
        if (!postRequest(body))
            return false; // 网络错误已经在 postRequest 内重试过

        AgentResponse parsed{};
        if (parseApiResponse(body.c_str(), parsed))
        {
            response   = parsed;
            _lastError = "";
            appendHistory(historyText, response.reply_text);
            Serial.println("[sys] agent: API call completed successfully");
            return true;
        }

        if (attempt < AGENT_PARSE_RETRIES)
        {
            Serial.printf("[warn] agent: reply not usable (%s), retrying with strict params\n", _lastError.c_str());
            delay(AGENT_RETRY_DELAY);
        }
    }

    return false;
}
