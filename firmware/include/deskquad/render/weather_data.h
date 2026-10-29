// weather_data.h 定义天气页需要的数据
//
// 取数在 app/weather_service.h，绘制在 render/views.h。这里只放两者
// 共用的数据结构，避免渲染层反过来依赖服务层。

#pragma once

#include <Arduino.h>

// WeatherData 是一次天气查询的结果
struct WeatherData
{
    String temperature; // 最高温度
    String humidity;    // 湿度
    String weather;     // 天气描述，如 "晴"、"小雨"
    bool ready;         // 是否已成功取到数据
};
