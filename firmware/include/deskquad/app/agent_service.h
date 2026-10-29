// agent_service.h 管理 AI 请求的生命周期
//
// Web、语音、手势、串口四条入口都汇到 Submit。Submit 只负责入队，
// 由主循环里的 Pump 串行执行——HTTP 调用会阻塞数秒，但在 loop 任务里
// 执行可以保证动作、显示与舵机不会被 Web 任务并发修改。
//
// Web 任务只读写本类自己的状态，统一由互斥锁保护；融合层、情感引擎、
// 运动与显示只在 Pump 里访问，始终运行在 loop 任务中。

#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include <deskquad/agent/agent_types.h>
#include <deskquad/agent/deepseek_agent.h>
#include <deskquad/emotion/emotion_engine.h>
#include <deskquad/emotion/multimodal_fusion.h>
#include <deskquad/hal/speaker.h>
#include <deskquad/motion/motion_director.h>
#include <deskquad/render/expression_renderer.h>

// AgentSnapshot 是一次请求状态的快照
// Web 接口一次读取全部字段，避免分多次读到中间状态
struct AgentSnapshot
{
    String status; // idle / queued / processing / completed / failed
    String requestId;
    String reply;
    String error;
    bool busy;
};

// AgentService 编排一次 AI 对话
class AgentService
{
public:
    // Begin 关联各子系统，同时创建互斥锁
    void Begin(DeepSeekAgent &agent, MultimodalFusion &fusion, EmotionEngine &emotion, MotionDirector &motion,
               Speaker &speaker, ExpressionRenderer &renderer);

    // Submit 提交一条用户输入
    //
    // # Params:
    //
    //	text: 用户输入文本
    //	contextReady: 为 true 时复用融合层已有的上下文，不再重复更新情感
    //
    // # Returns:
    //
    //	request_id；设备忙或 AI 未配置时返回空串
    String Submit(const String &text, bool contextReady = false);

    // Pump 处理待发送的请求，需在每次主循环中调用
    void Pump();

    // ClearHistory 清空对话历史与最近一次回复
    void ClearHistory();

    // Snapshot 读取当前请求状态
    AgentSnapshot Snapshot();

private:
    void dispatch(const AgentResponse &response);
    void lock();
    void unlock();

    // 指针默认置空，Begin 之前调用任何接口都不会解引用野指针
    DeepSeekAgent *_agent         = nullptr;
    MultimodalFusion *_fusion     = nullptr;
    EmotionEngine *_emotion       = nullptr;
    MotionDirector *_motion       = nullptr;
    Speaker *_speaker             = nullptr;
    ExpressionRenderer *_renderer = nullptr;

    SemaphoreHandle_t _mutex = nullptr;

    AgentState _state = AGENT_IDLE;
    String _pendingText;
    String _pendingRequestId;
    String _lastRequestId;
    String _lastReply;
    String _lastError;
    String _resultStatus      = "idle"; // idle / queued / processing / completed / failed
    bool _pendingInput        = false;
    bool _pendingContextReady = false;
    bool _clearHistoryPending = false; // 请求进行中收到的清空请求，结束后再执行
    unsigned long _cooldownMs = 0;
    uint32_t _requestSeq      = 0;
};
