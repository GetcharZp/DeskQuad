// battery_math.h 提供电量的纯计算
//
// 不依赖 Arduino，可以被 env:native 直接编译测试。

#pragma once

#include <stdint.h>

// BatteryAverager 维护一个定长滑动窗口
class BatteryAverager
{
public:
    static constexpr uint8_t kMaxSamples = 32;

    // 构造函数
    //
    // # Params:
    //
    //	capacity: 窗口长度，上限为 kMaxSamples
    explicit BatteryAverager(uint8_t capacity);

    // Push 加入一个采样值
    void Push(uint16_t sample);

    // Average 返回窗口内样本均值，窗口为空时返回 0
    float Average() const;

    // IsFull 判断窗口是否已填满
    bool IsFull() const { return _count >= _capacity; }

    void Reset();

private:
    uint16_t _samples[kMaxSamples];
    uint8_t _capacity;
    uint8_t _count;
    uint8_t _next;
};

// MapBatteryPercentage 把电池电压换算为电量百分比
//
// 低于 minVoltage 为 0%，高于 maxVoltage 为 100%，区间内线性插值。
//
// # Params:
//
//	voltage: 电池电压 (V)
//	minVoltage: 0% 对应的电压
//	maxVoltage: 100% 对应的电压
//
// # Returns:
//
//	电量百分比 0-100
int MapBatteryPercentage(float voltage, float minVoltage, float maxVoltage);
