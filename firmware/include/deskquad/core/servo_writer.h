// servo_writer.h 定义舵机写入接口
//
// 运动层只依赖这个接口，真实硬件由 hal/servo_bus.h 实现。

#pragma once

#include <stdint.h>

// IServoWriter 是运动层写舵机的唯一出口
struct IServoWriter
{
    virtual ~IServoWriter() {}

    // Write 把指定通道转到目标角度
    //
    // # Params:
    //
    //	channel: 通道号 0-3，依次对应 Servo1-4
    //	angle:   目标角度 0-180
    virtual void Write(uint8_t channel, int16_t angle) = 0;
};
