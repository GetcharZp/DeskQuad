// views.h 定义三个非表情页面：断网提示、天气、时间
//
// 这一层只负责把内容画进当前显示缓冲，不清屏也不提交；页面切换与
// 状态提示统一由 ExpressionRenderer 处理，因此这些视图可以放心地
// 在局部被遮挡。
//
// 数据由 app/weather_service 与 hal/time_service 提供。

#pragma once

#include <deskquad/hal/display.h>
#include <deskquad/hal/time_service.h>
#include <deskquad/render/weather_data.h>

// NetworkView 是未联网时显示的提示页
class NetworkView
{
public:
    // Draw 把断网提示画进当前缓冲
    void Draw(IDisplay &display) const;
};

// WeatherView 显示天气
class WeatherView
{
public:
    // Draw 把天气页画进当前缓冲
    //
    // # Params:
    //
    //	display: 显示接口
    //	data: 天气数据，未就绪时显示默认图标与空值
    //	wifiConnected: 未联网时改画断网提示
    void Draw(IDisplay &display, const WeatherData &data, bool wifiConnected) const;
};

// ClockView 显示时间
class ClockView
{
public:
    // Draw 把时间页画进当前缓冲
    //
    // # Params:
    //
    //	display: 显示接口
    //	timeService: 时间来源
    //	wifiConnected: 未联网时改画断网提示
    void Draw(IDisplay &display, const TimeService &timeService, bool wifiConnected) const;
};
