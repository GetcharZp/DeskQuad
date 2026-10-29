#include <deskquad/hal/battery.h>

#include <Arduino.h>

#include <deskquad/config/pins.h>
#include <deskquad/core/clock.h>

namespace
{
    constexpr unsigned long kSampleIntervalMs = 10;
} // namespace

BatteryMonitor::BatteryMonitor(const deskquad::Config &config)
    : _config(config), _averager(config.adcWindowSize), _lastSampleMs(0), _voltage(0.0f), _percentage(0)
{
}

void BatteryMonitor::Begin()
{
    _averager.Reset();
    _lastSampleMs = 0;
}

void BatteryMonitor::Update()
{
    const unsigned long now = NowMs();
    if (now - _lastSampleMs < kSampleIntervalMs)
        return;
    _lastSampleMs = now;

    // analogReadMilliVolts 使用芯片校准数据，不受 ADC 位数与参考电压影响
    _averager.Push(static_cast<uint16_t>(analogReadMilliVolts(deskquad::kAdcPin)));
    if (!_averager.IsFull())
        return;

    _voltage    = _averager.Average() / 1000.0f * _config.adcDividerRatio;
    _percentage = MapBatteryPercentage(_voltage, _config.batteryMinVoltage, _config.batteryMaxVoltage);
}
