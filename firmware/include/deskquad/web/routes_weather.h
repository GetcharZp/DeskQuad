// routes_weather.h 注册天气配置路由

#pragma once

#include <ESPAsyncWebServer.h>

#include <deskquad/app/app_context.h>

// RegisterWeatherRoutes 注册 /api/weatherConfig（GET 回显配置、POST 保存）
void RegisterWeatherRoutes(AsyncWebServer &server, AppContext &context);
