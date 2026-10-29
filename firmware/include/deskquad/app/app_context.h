// app_context.h 把各子系统打包传给 Web 路由
//
// 路由模块只需要这一个参数，不必知道全局对象是怎么构造的。

#pragma once

#include <deskquad/agent/deepseek_agent.h>
#include <deskquad/app/agent_service.h>
#include <deskquad/app/runtime_state.h>
#include <deskquad/app/weather_service.h>
#include <deskquad/config/app_config.h>
#include <deskquad/emotion/emotion_engine.h>
#include <deskquad/emotion/multimodal_fusion.h>
#include <deskquad/hal/battery.h>
#include <deskquad/hal/settings_store.h>
#include <deskquad/hal/speaker.h>
#include <deskquad/motion/motion_director.h>
#include <deskquad/perception/gesture_recognizer.h>
#include <deskquad/perception/hlkv20.h>
#include <deskquad/perception/ov2640.h>
#include <deskquad/render/expression_renderer.h>

// AppContext 是各子系统引用的集合
struct AppContext
{
    const deskquad::Config &config;    // 运行配置
    deskquad::SettingsStore &settings; // 持久化配置 (NVS)
    MotionDirector &motion;            // 运动调度
    ExpressionRenderer &renderer;      // 显示渲染
    Speaker &speaker;                  // 扬声器
    BatteryMonitor &battery;           // 电池
    AgentService &agent;               // AI 对话
    DeepSeekAgent &deepseek;           // AI 客户端
    EmotionEngine &emotion;            // 情感状态
    MultimodalFusion &fusion;          // 多模态输入融合
    GestureRecognizer &gesture;        // 手势识别
    HLKV20 &voice;                     // 语音识别
    Camera &camera;                    // 摄像头
    WeatherService &weather;           // 天气
    RuntimeState &runtime;             // 交互状态
};
