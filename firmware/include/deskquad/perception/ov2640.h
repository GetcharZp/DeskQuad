// ov2640.h 摄像头采集 (DVP OV2640 + esp32-camera)
//
// 传感器固定输出 VGA (640×480) 的 YUV422 数据：
//   - 手势识别：8×8 下采样成 80×60 灰度图；
//   - 拍照：用 frame2jpg() 软件编码成 640×480 JPEG。
// 手势与拍照共用同一帧源，运行时不做传感器格式切换。

#pragma once

#include <Arduino.h>
#include <esp_camera.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include <deskquad/config/pins.h>

// Camera 通过 DVP 并口抓取 OV2640 画面
class Camera
{
public:
    // Begin 初始化 DVP 并口、SCCB 与传感器
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

    // CaptureJpeg 从当前 YUV422 帧软件编码一张 JPEG
    //
    // 不切换传感器格式，直接从 VGA 帧调用 frame2jpg()，因此不会影响
    // 手势识别的背景模型和曝光。与 Poll 共用同一把锁，编码期间其他
    // 抓帧会等待。成功时 *buffer 指向新分配的 JPEG，调用方用 free() 释放。
    //
    // # Params:
    //
    //	buffer: 输出指针，成功时指向新分配的 JPEG 数据
    //	length: 输出长度，单位字节
    //
    // # Returns:
    //
    //	true 表示 *buffer 与 *length 已更新
    bool CaptureJpeg(uint8_t **buffer, size_t *length);

    // SetIntervalMs 设置 Poll 的抓帧间隔 (ms)
    void SetIntervalMs(uint32_t ms) { _intervalMs = ms; }

    // IsAvailable 返回传感器是否初始化成功
    bool IsAvailable() const { return _available; }

private:
    // captureGray 抓一帧并下采样为灰度图，调用前必须持有互斥锁
    bool captureGray(uint8_t *buffer);

    // readFrame 从 YUV422/GRAYSCALE 帧中提取亮度并下采样
    bool readFrame(const camera_fb_t *frame, uint8_t *buffer);

    // waitFrame 等待驱动队列里出现一帧，超时返回 nullptr
    camera_fb_t *waitFrame(uint32_t timeoutMs);

    // lock/unlock 串行化主循环与 Web 任务对摄像头的访问
    bool lock(uint32_t timeoutMs);
    void unlock();

    SemaphoreHandle_t _lock = nullptr;
    uint32_t _intervalMs    = 200;
    uint32_t _lastPollMs    = 0;
    bool _available         = false;
};
