// routes_control.h 注册遥控与状态查询路由
//
// 包括姿态、表情、自由模式、电池与各外设的在线状态。

#pragma once

#include <ESPAsyncWebServer.h>

#include <deskquad/app/app_context.h>

// RegisterControlRoutes 注册 /api/front、/api/happy、/api/batteryVoltage 等路由
void RegisterControlRoutes(AsyncWebServer &server, AppContext &context);
