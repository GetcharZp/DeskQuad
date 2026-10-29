// ov2640.h 摄像头采集 (ArduCAM Mini 2MP Plus + OV2640)
//
// 传感器固定输出 160×120 RGB565 原始帧，抓帧后直接 2×2 下采样成 80×60
// 灰度图，作为 GestureRecognizer 的输入。这里不做 JPEG 编解码，也不
// 保留彩图，因此初始化之后就不再切换传感器格式。

#pragma once

#include <ArduCAM.h>
#include <Arduino.h>
#include <SPI.h>
#include <deskquad/config/pins.h>

// Camera 通过 SPI 抓取 OV2640 画面
class Camera
{
public:
    // Begin 初始化 SPI、SCCB 与传感器
    //
    // # Returns:
    //
    //	true 表示传感器已就绪；失败后 IsAvailable 始终为 false
    bool Begin();

    // Capture 立即抓取一帧并下采样为 80×60 灰度图
    //
    // # Params:
    //
    //	buffer: 长度 kGesturePixelCount 的输出缓冲
    //
    // # Returns:
    //
    //	true 表示 buffer 已被整帧覆盖
    bool Capture(uint8_t *buffer);

    // Poll 按采样间隔抓帧，未到间隔时直接返回
    //
    // # Params:
    //
    //	buffer: 长度 kGesturePixelCount 的输出缓冲
    //
    // # Returns:
    //
    //	true 表示本次写入了新帧
    bool Poll(uint8_t *buffer);

    // SetIntervalMs 设置 Poll 的抓帧间隔 (ms)
    void SetIntervalMs(uint32_t ms) { _intervalMs = ms; }

    // IsAvailable 返回传感器是否初始化成功
    bool IsAvailable() const { return _available; }

private:
    // waitCaptureDone 等待传感器把一帧写进 FIFO
    bool waitCaptureDone();

    // readFrame 从 FIFO 读出整帧并下采样到灰度缓冲
    void readFrame(uint8_t *buffer);

    ArduCAM _cam{OV2640, deskquad::kCameraCsPin};
    uint32_t _intervalMs = 200;
    uint32_t _lastPollMs = 0;
    bool _available      = false;
};
