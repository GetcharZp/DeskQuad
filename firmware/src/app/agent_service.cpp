#include <deskquad/app/agent_service.h>

#include <deskquad/core/states.h>

namespace
{
    const char *kRequestIdPrefix = "dq-";

    // 请求失败时的设备端反馈：一句中性提示 + 错误表情 + 错误提示音，
    // 不执行动作、不改情感。提示不区分具体原因（密钥、网络、解析都会走到这里），
    // 真实失败原因由 Web 端从 /api/aiStatus 的 error 字段读取。
    AgentResponse makeFailureResponse()
    {
        AgentResponse response;
        response.reply_text = "出了点问题，请稍后再试。";
        response.action     = -1;
        response.expression = static_cast<int8_t>(EmojiId::Error);
        response.melody     = static_cast<int8_t>(MELODY_ERROR);
        response.repeat     = 1;
        response.emotion    = 0;
        return response;
    }
} // namespace

void AgentService::Begin(DeepSeekAgent &agent, MultimodalFusion &fusion, EmotionEngine &emotion, MotionDirector &motion,
                         Speaker &speaker, ExpressionRenderer &renderer)
{
    _agent    = &agent;
    _fusion   = &fusion;
    _emotion  = &emotion;
    _motion   = &motion;
    _speaker  = &speaker;
    _renderer = &renderer;

    if (_mutex == nullptr)
        _mutex = xSemaphoreCreateMutex();

    lock();
    _state        = AGENT_IDLE;
    _resultStatus = "idle";
    _pendingInput = false;
    _cooldownMs   = 0;
    _requestSeq   = 0;
    unlock();
}

void AgentService::lock()
{
    if (_mutex != nullptr)
        xSemaphoreTake(_mutex, portMAX_DELAY);
}

void AgentService::unlock()
{
    if (_mutex != nullptr)
        xSemaphoreGive(_mutex);
}

// 只入队，真正的上下文构建与网络请求都在 Pump 里完成
String AgentService::Submit(const String &text, bool contextReady)
{
    if (text.length() == 0 || _agent == nullptr || !_agent->isConfigured())
        return String();

    String requestId;
    lock();
    if (_state != AGENT_IDLE || _pendingInput)
    {
        unlock();
        Serial.println("[warn] agent: busy, input ignored");
        return String();
    }

    _requestSeq++;
    if (_requestSeq == 0)
        _requestSeq = 1;

    _pendingText         = text;
    _pendingContextReady = contextReady;
    _pendingRequestId    = String(kRequestIdPrefix) + String(_requestSeq);
    _lastRequestId       = _pendingRequestId;
    _lastReply           = "";
    _lastError           = "";
    _resultStatus        = "queued";
    _pendingInput        = true;
    requestId            = _pendingRequestId;
    unlock();

    return requestId;
}

void AgentService::Pump()
{
    if (_agent == nullptr || millis() - _cooldownMs <= AGENT_COOLDOWN_MS)
        return;

    String text;
    bool contextReady;
    lock();
    if (!_pendingInput || _state != AGENT_IDLE)
    {
        unlock();
        return;
    }
    _state                 = AGENT_BUSY;
    _pendingInput          = false;
    text                   = _pendingText;
    contextReady           = _pendingContextReady;
    _resultStatus          = "processing";
    const String requestId = _pendingRequestId;
    unlock();

    // 融合层与情感引擎只在 loop 任务里访问
    MultimodalContext context;
    if (contextReady)
    {
        context          = _fusion->getLastContext();
        context.userText = text;
    }
    else
    {
        context = _fusion->processText(text);
    }

    // 情感上下文在这里拼进用户消息，Agent 只负责发送
    const String prompt = _fusion->buildAffectivePrompt(context);

    const PADState &pad = _emotion->GetState();
    _motion->SetEmotionalIntensity(pad.pleasure, pad.arousal);

    Serial.printf("[sys] agent: processing %s \"%s\"\n", requestId.c_str(), context.userText.c_str());

    AgentResponse response;
    const bool ok = _agent->ask(prompt, context.userText, response);

    if (ok)
    {
        dispatch(response);
    }
    else
    {
        dispatch(makeFailureResponse());
    }

    lock();
    if (_clearHistoryPending)
    {
        // 请求途中用户清空了对话：丢弃这次结果，只把历史清掉
        _agent->clearHistory();
        _clearHistoryPending = false;
    }
    else if (ok)
    {
        _lastReply    = response.reply_text;
        _lastError    = "";
        _resultStatus = "completed";
    }
    else
    {
        _lastReply    = "";
        _lastError    = _agent->getLastError();
        _resultStatus = "failed";
        Serial.printf("[warn] agent: request failed: %s\n", _lastError.c_str());
    }

    _state      = AGENT_IDLE;
    _cooldownMs = millis();
    unlock();
}

void AgentService::ClearHistory()
{
    lock();
    _lastReply           = "";
    _lastRequestId       = "";
    _lastError           = "";
    _resultStatus        = "idle";
    _pendingInput        = false; // 取消还没开始的请求
    const bool busy      = (_state == AGENT_BUSY);
    _clearHistoryPending = busy; // 进行中的请求结束后再清历史
    unlock();

    // 没有请求在跑时直接清；否则留给 Pump 在请求结束后清
    if (!busy && _agent != nullptr)
        _agent->clearHistory();
}

AgentSnapshot AgentService::Snapshot()
{
    AgentSnapshot snapshot;
    lock();
    snapshot.status    = _resultStatus;
    snapshot.requestId = _lastRequestId;
    snapshot.reply     = _lastReply;
    snapshot.error     = _lastError;
    snapshot.busy      = (_state == AGENT_BUSY || _pendingInput);
    unlock();
    return snapshot;
}

void AgentService::dispatch(const AgentResponse &response)
{
    Serial.printf("[sys] agent: reply=\"%s\" action=%d expr=%d melody=%d repeat=%d\n", response.reply_text.c_str(),
                  response.action, response.expression, response.melody, response.repeat);

    const ActionDecision decision = DecideAction(response.action);
    if (decision.kind == ActionDecision::Kind::Emotion)
    {
        _motion->TriggerEmotion(decision.emotion, max((uint8_t)1, response.repeat));
    }
    else if (decision.kind == ActionDecision::Kind::Pose)
    {
        _motion->RequestPose(decision.pose);
    }

    if (response.expression >= 0 && response.expression <= 9)
    {
        _renderer->SetEmoji(static_cast<EmojiId>(response.expression));
    }

    if (response.melody >= 0 && response.melody < static_cast<int8_t>(MELODY_COUNT))
    {
        _speaker->play(static_cast<Melody>(response.melody));
    }

    if (response.emotion >= 1 && response.emotion <= 5)
    {
        _emotion->UpdateFromEmotion(static_cast<AgentEmotion>(response.emotion));
        const PADState &pad = _emotion->GetState();
        _motion->SetEmotionalIntensity(pad.pleasure, pad.arousal);
    }
}
