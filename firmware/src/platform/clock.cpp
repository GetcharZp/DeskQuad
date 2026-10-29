// clock.cpp 提供 core/clock.h 的平台实现
//
// 设备上直接读 millis()；native 测试用一个可手动推进的计数器，
// 这样 core 层不需要感知 Arduino，也不必让每个测试各自打桩。

#include <deskquad/core/clock.h>

#ifdef ARDUINO

#include <Arduino.h>

unsigned long NowMs()
{
    return millis();
}

#else

namespace
{
    unsigned long gNowMs = 0;
}

unsigned long NowMs()
{
    return gNowMs;
}

// 以下两个接口只在 native 构建中提供，供单元测试控制时间
void SetNowMs(unsigned long ms)
{
    gNowMs = ms;
}

void AdvanceNowMs(unsigned long ms)
{
    gNowMs += ms;
}

#endif
