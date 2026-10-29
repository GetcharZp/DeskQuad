// web_router.h 汇总全部 Web 路由

#pragma once

#include <ESPAsyncWebServer.h>

#include <deskquad/app/app_context.h>

// RegisterAllRoutes 注册全部路由并启动服务器
//
// 只应调用一次：重复注册会让同一路径挂上多个 handler，内存翻倍且行为不确定。
//
// # Params:
//
//	server: Web 服务器
//	context: 各子系统引用
void RegisterAllRoutes(AsyncWebServer &server, AppContext &context);
