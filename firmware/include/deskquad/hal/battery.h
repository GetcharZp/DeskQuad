// battery.h 采样电池电压
//
// 采样是非阻塞的：每次 Update 最多读一次 ADC，凑满窗口后才发布新读数。
// 原实现一次读十遍、中间各 delay(10)，每轮主循环白白阻塞 100 毫秒。

#pragma once

#include <deskquad/config/app_config.h>
#include <deskquad/core/battery_math.h>

// BatteryMonitor 通过分压电阻测量电池电压
class BatteryMonitor
{
public:
    // 构造函数
    //
    // # Params:
    //
    //	config: 运行配置，提供采样引脚、窗口长度与分压比
    explicit BatteryMonitor(const deskquad::Config &config);

    void Begin();

    // Update 推进一次采样，需在每次主循环中调用
    void Update();

    // Voltage 返回最近一次发布的电池电压 (V)
    float Voltage() const { return _voltage; }

    // Percentage 返回最近一次发布的电量百分比
    int Percentage() const { return _percentage; }

private:
    const deskquad::Config &_config;
    BatteryAverager _averager;
    unsigned long _lastSampleMs;
    float _voltage;
    int _percentage;
};
