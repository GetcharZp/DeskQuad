// routes_voice.h 注册豆包语音配置与测试路由

#pragma once

#include <ESPAsyncWebServer.h>

#include <deskquad/app/app_context.h>

// RegisterVoiceRoutes 注册 /api/voiceConfig 与 /api/voice/test
void RegisterVoiceRoutes(AsyncWebServer &server, AppContext &context);
