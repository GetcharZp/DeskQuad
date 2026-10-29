// views.h 定义三个非表情页面：断网提示、天气、时间
//
// 这一层只负责画，数据由 app/weather_service 与 hal/time_service 提供。

#pragma once

#include <deskquad/hal/display.h>
#include <deskquad/hal/time_service.h>
#include <deskquad/render/weather_data.h>

// NetworkView 是未联网时显示的提示页
class NetworkView
{
public:
    void Render(IDisplay &display) const;
};

// WeatherView 显示天气
class WeatherView
{
public:
    // Render 绘制天气页
    //
    // # Params:
    //
    //	display: 显示接口
    //	data: 天气数据，未就绪时显示默认图标与空值
    //	wifiConnected: 未联网时改画断网提示
    void Render(IDisplay &display, const WeatherData &data, bool wifiConnected) const;
};

// ClockView 显示时间
class ClockView
{
public:
    // Render 绘制时间页
    //
    // # Params:
    //
    //	display: 显示接口
    //	timeService: 时间来源
    //	wifiConnected: 未联网时改画断网提示
    void Render(IDisplay &display, const TimeService &timeService, bool wifiConnected) const;
};
