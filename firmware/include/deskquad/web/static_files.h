// static_files.h 提供设备端静态资源的路由注册

#pragma once

#include <ESPAsyncWebServer.h>

// RegisterStaticFiles 注册 Vue 单页应用的入口、/assets/ 静态资源
//
// LittleFS 根目录由 platformio.ini 的 data_dir 指向 web/dist。
void RegisterStaticFiles(AsyncWebServer &server);
