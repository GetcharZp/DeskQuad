#include <deskquad/core/battery_math.h>

BatteryAverager::BatteryAverager(uint8_t capacity) : _capacity(capacity), _count(0), _next(0)
{
    if (_capacity > kMaxSamples)
        _capacity = kMaxSamples;
    if (_capacity == 0)
        _capacity = 1;
    Reset();
}

void BatteryAverager::Reset()
{
    for (uint8_t i = 0; i < kMaxSamples; i++)
        _samples[i] = 0;
    _count = 0;
    _next  = 0;
}

void BatteryAverager::Push(uint16_t sample)
{
    _samples[_next] = sample;
    _next           = (_next + 1) % _capacity;
    if (_count < _capacity)
        _count++;
}

float BatteryAverager::Average() const
{
    if (_count == 0)
        return 0.0f;
    uint32_t total = 0;
    for (uint8_t i = 0; i < _count; i++)
        total += _samples[i];
    return static_cast<float>(total) / static_cast<float>(_count);
}

int MapBatteryPercentage(float voltage, float minVoltage, float maxVoltage)
{
    if (voltage <= minVoltage)
        return 0;
    if (voltage >= maxVoltage)
        return 100;
    if (maxVoltage <= minVoltage)
        return 0;
    return static_cast<int>((voltage - minVoltage) / (maxVoltage - minVoltage) * 100.0f);
}
