// routes_agent.h 注册 AI 对话相关路由

#pragma once

#include <ESPAsyncWebServer.h>

#include <deskquad/app/app_context.h>

// RegisterAgentRoutes 注册 /api/aitalk、/api/aiStatus、/api/aiConfig 等路由
void RegisterAgentRoutes(AsyncWebServer &server, AppContext &context);
