// servo_bus.h 封装四路舵机

#pragma once

#include <ESP32Servo.h>

#include <deskquad/core/servo_writer.h>

// ServoBus 管理四路舵机
class ServoBus : public IServoWriter
{
public:
    // Begin 挂载四路舵机并回到中立位
    void Begin();

    // Write 把指定通道转到目标角度
    //
    // # Params:
    //
    //	channel: 通道号 0-3，越界调用会被忽略
    //	angle:   目标角度，自动限制在 0-180
    void Write(uint8_t channel, int16_t angle) override;

private:
    Servo _servos[4];
};
