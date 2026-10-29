#include <deskquad/hal/buttons.h>

#include <Arduino.h>

#include <deskquad/config/pins.h>

namespace
{

    constexpr unsigned long kDebounceMs = 50;

    volatile bool gPressedFirst  = false;
    volatile bool gPressedSecond = false;
    unsigned long gLastFirstMs   = 0;
    unsigned long gLastSecondMs  = 0;

} // namespace

// 中断服务程序不能使用 String、delay 或任何可能阻塞的调用
void IRAM_ATTR handleButtonPress()
{
    const unsigned long now = millis();
    if (now - gLastFirstMs > kDebounceMs)
    {
        gPressedFirst = true;
        gLastFirstMs  = now;
    }
    if (now - gLastSecondMs > kDebounceMs)
    {
        gPressedSecond = true;
        gLastSecondMs  = now;
    }
}

void Buttons::Begin()
{
    pinMode(deskquad::kButtonPins[0], INPUT_PULLUP);
    pinMode(deskquad::kButtonPins[1], INPUT);
    attachInterrupt(digitalPinToInterrupt(deskquad::kButtonPins[0]), handleButtonPress, FALLING);
    attachInterrupt(digitalPinToInterrupt(deskquad::kButtonPins[1]), handleButtonPress, RISING);
}

bool Buttons::ConsumeFirst()
{
    if (!gPressedFirst)
        return false;
    gPressedFirst = false;
    return true;
}

bool Buttons::ConsumeSecond()
{
    if (!gPressedSecond)
        return false;
    gPressedSecond = false;
    return true;
}
