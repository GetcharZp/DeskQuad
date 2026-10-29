#include <deskquad/hal/servo_bus.h>

#include <deskquad/config/pins.h>

namespace
{
    constexpr int16_t kNeutralAngle = 90;
    constexpr int kMinPulseUs       = 500;
    constexpr int kMaxPulseUs       = 2500;
    constexpr uint8_t kServoCount   = 4;
} // namespace

void ServoBus::Begin()
{
    for (uint8_t i = 0; i < kServoCount; i++)
    {
        _servos[i].attach(deskquad::kServoPins[i], kMinPulseUs, kMaxPulseUs);
        _servos[i].write(kNeutralAngle);
    }
}

void ServoBus::Write(uint8_t channel, int16_t angle)
{
    if (channel >= kServoCount)
        return;
    _servos[channel].write(constrain(angle, 0, 180));
}
