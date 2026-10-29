// routes_network.h 注册配网路由

#pragma once

#include <ESPAsyncWebServer.h>

#include <deskquad/app/app_context.h>

// RegisterNetworkRoute 注册 /api/wifiConfig（GET 回显配置、POST 保存）、/api/networkStatus
void RegisterNetworkRoute(AsyncWebServer &server, AppContext &context);
