// OV2640 DVP 摄像头实现
#include <deskquad/perception/ov2640.h>

#include <esp_heap_caps.h>

#include <cstdlib>
#include <cstring>

#include <deskquad/perception/gesture_recognizer.h>

namespace
{

    // XCLK 由 LEDC 产生。OV2640 常规工作在 20MHz；如果排线较长导致
    // 画面出现横纹，可以降到 10MHz。
    constexpr int kXclkFrequencyHz = 20000000;

    // 传感器固定输出 VGA YUV422。拍照用 frame2jpg() 软件编码，
    // 质量参数范围是 0-100（越大越好），与传感器 JPEG 的 0-63 不同。
    // 如果 VGA 帧缓冲分配失败，可改成 FRAMESIZE_QVGA (320×240)，
    // 并把 kSensorWidth/Height 改成 320/240。
    constexpr framesize_t kCameraFrameSize = FRAMESIZE_VGA;
    constexpr uint8_t kPhotoJpegQuality     = 80;
    constexpr size_t kMaxPhotoBytes         = 512 * 1024;

    // 手势帧是 640×480 YUV422，8×8 下采样成 80×60 灰度
    constexpr int kSensorWidth  = 640;
    constexpr int kSensorHeight = 480;
    constexpr int kFrameWidth   = deskquad::kGestureFrameWidth;
    constexpr int kFrameHeight  = deskquad::kGestureFrameHeight;
    constexpr int kStepX        = kSensorWidth / kFrameWidth;
    constexpr int kStepY        = kSensorHeight / kFrameHeight;

    static_assert(kSensorWidth % kFrameWidth == 0 && kSensorHeight % kFrameHeight == 0,
                  "灰度帧尺寸必须整除传感器输出尺寸");

    // 等画面的超时：VGA 帧正常帧率下几百毫秒内到达
    constexpr uint32_t kFirstFrameTimeoutMs = 2000;
    constexpr uint32_t kCaptureTimeoutMs    = 300;
    constexpr uint32_t kPhotoTimeoutMs      = 1000;

    // 主循环与 Web 任务共用摄像头，拍照编码期间用互斥锁串行化
    constexpr uint32_t kLockTimeoutMs = 3000;

    // JpegBuffer 是软件 JPEG 编码的输出目标，直接落在 PSRAM，避免占用内部 RAM
    struct JpegBuffer
    {
        uint8_t *data     = nullptr;
        size_t capacity   = 0;
        size_t length     = 0;
        bool overflow     = false;
    };

    // jpegWriteCallback 按顺序把编码器输出的字节追加到 PSRAM 缓冲
    size_t jpegWriteCallback(void *arg, size_t /*index*/, const void *data, size_t len)
    {
        JpegBuffer *out = static_cast<JpegBuffer *>(arg);
        if (out == nullptr || data == nullptr)
            return 0;
        if (out->length + len > out->capacity)
        {
            out->overflow = true;
            return 0;
        }
        memcpy(out->data + out->length, data, len);
        out->length += len;
        return len;
    }

    // makeConfig 生成固定的 esp32-camera 配置
    camera_config_t makeConfig()
    {
        camera_config_t config = {};

        config.pin_pwdn     = deskquad::kCameraPinPwdn;
        config.pin_reset    = deskquad::kCameraPinReset;
        config.pin_xclk     = deskquad::kCameraPinXclk;
        config.pin_sccb_sda = deskquad::kCameraPinSiod;
        config.pin_sccb_scl = deskquad::kCameraPinSioc;
        config.pin_vsync    = deskquad::kCameraPinVsync;
        config.pin_href     = deskquad::kCameraPinHref;
        config.pin_pclk     = deskquad::kCameraPinPclk;
        config.pin_d0       = deskquad::kCameraPinD0;
        config.pin_d1       = deskquad::kCameraPinD1;
        config.pin_d2       = deskquad::kCameraPinD2;
        config.pin_d3       = deskquad::kCameraPinD3;
        config.pin_d4       = deskquad::kCameraPinD4;
        config.pin_d5       = deskquad::kCameraPinD5;
        config.pin_d6       = deskquad::kCameraPinD6;
        config.pin_d7       = deskquad::kCameraPinD7;

        config.xclk_freq_hz   = kXclkFrequencyHz;
        config.ledc_timer     = LEDC_TIMER_0;
        config.ledc_channel   = LEDC_CHANNEL_0;
        config.pixel_format   = PIXFORMAT_YUV422;
        config.frame_size     = kCameraFrameSize;
        config.jpeg_quality   = 12; // 仅 PIXFORMAT_JPEG 时有效，这里保持默认
        // 双缓冲让手势帧更接近实时，也避免单缓冲时 DMA 等待
        config.fb_count       = 2;
        config.fb_location    = CAMERA_FB_IN_PSRAM;
        config.grab_mode        = CAMERA_GRAB_LATEST;
        config.sccb_i2c_port    = -1;
        config.jpeg_buffer_size = 0;
        return config;
    }

} // namespace

bool Camera::Begin()
{
    // 主循环和 Web 任务都会访问摄像头，先准备好互斥锁（只创建一次）
    if (_lock == nullptr)
    {
        _lock = xSemaphoreCreateMutex();
        if (_lock == nullptr)
        {
            Serial.println("[warn] camera: mutex allocation failed");
            return false;
        }
    }

    // 允许重复调用 Begin：上一次失败可能留下未释放的驱动状态
    esp_camera_deinit();

    camera_config_t config = makeConfig();
    esp_err_t err          = esp_camera_init(&config);
    if (err != ESP_OK)
    {
        Serial.printf("[warn] camera: init failed: %s\n", esp_err_to_name(err));
        return false;
    }

    // 等第一帧，确认数据线与时序正常，同时丢弃上电后的脏帧
    camera_fb_t *frame = waitFrame(kFirstFrameTimeoutMs);
    if (frame == nullptr)
    {
        Serial.println("[warn] camera: no frame after init");
        esp_camera_deinit();
        return false;
    }
    esp_camera_fb_return(frame);

    _available = true;
    return true;
}

bool Camera::Capture(uint8_t *buffer)
{
    if (!_available || buffer == nullptr)
        return false;
    // 主循环不等待 Web 拍照：拿不到锁就跳过这一帧，避免表情和舵机卡顿
    if (!lock(0))
        return false;

    // 等锁期间拍照失败可能把驱动标记为不可用
    if (!_available)
    {
        unlock();
        return false;
    }

    const bool ok = captureGray(buffer);
    unlock();
    return ok;
}

bool Camera::CaptureJpeg(uint8_t **buffer, size_t *length)
{
    if (!_available || buffer == nullptr || length == nullptr)
        return false;
    if (!lock(kLockTimeoutMs))
        return false;

    if (!_available)
    {
        unlock();
        return false;
    }

    uint8_t *photo     = nullptr;
    size_t photoLength = 0;
    bool ok            = false;

    // 直接取当前 VGA YUV422 帧做软件 JPEG 编码，不做传感器格式切换。
    camera_fb_t *frame = waitFrame(kPhotoTimeoutMs);
    if (frame != nullptr)
    {
        JpegBuffer encoded;
        encoded.data = static_cast<uint8_t *>(
            heap_caps_malloc(kMaxPhotoBytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
        if (encoded.data != nullptr)
        {
            encoded.capacity = kMaxPhotoBytes;
            if (frame2jpg_cb(frame, kPhotoJpegQuality, jpegWriteCallback, &encoded) &&
                encoded.length > 0 && !encoded.overflow)
            {
                photo       = encoded.data;
                photoLength = encoded.length;
                ok          = true;
            }
            else
            {
                Serial.println("[warn] camera: JPEG encode failed");
                free(encoded.data);
            }
        }
        else
        {
            Serial.println("[warn] camera: photo buffer allocation failed");
        }
        esp_camera_fb_return(frame);
    }
    else
    {
        Serial.println("[warn] camera: photo capture timeout");
    }
    unlock();

    if (!ok)
        return false;

    *buffer = photo;
    *length = photoLength;
    return true;
}

bool Camera::Poll(uint8_t *buffer)
{
    if (!_available)
        return false;

    const uint32_t now = millis();
    if (now - _lastPollMs < _intervalMs)
        return false;

    // 失败也按间隔退避，避免抓帧失败时主循环被连续阻塞
    _lastPollMs = now;
    return Capture(buffer);
}

bool Camera::captureGray(uint8_t *buffer)
{
    camera_fb_t *frame = waitFrame(kCaptureTimeoutMs);
    if (frame == nullptr)
        return false;

    const bool ok = readFrame(frame, buffer);
    esp_camera_fb_return(frame);
    return ok;
}

bool Camera::readFrame(const camera_fb_t *frame, uint8_t *buffer)
{
    if (frame == nullptr || frame->buf == nullptr || buffer == nullptr)
        return false;

    if (frame->width != static_cast<size_t>(kSensorWidth) ||
        frame->height != static_cast<size_t>(kSensorHeight))
    {
        Serial.printf("[warn] camera: frame size %ux%u, expected %dx%d\n", static_cast<unsigned>(frame->width),
                      static_cast<unsigned>(frame->height), kSensorWidth, kSensorHeight);
        return false;
    }

    // YUV422 每个像素占 2 字节，亮度 Y 在像素对的第一个字节；
    // GRAYSCALE 则每个像素 1 字节，直接使用。
    int bytesPerPixel = 0;
    if (frame->format == PIXFORMAT_YUV422)
        bytesPerPixel = 2;
    else if (frame->format == PIXFORMAT_GRAYSCALE)
        bytesPerPixel = 1;
    else
    {
        Serial.printf("[warn] camera: unsupported frame format %d\n", static_cast<int>(frame->format));
        return false;
    }

    const size_t stride = static_cast<size_t>(frame->width) * bytesPerPixel;
    if (frame->len < stride * frame->height)
    {
        Serial.printf("[warn] camera: short frame %u bytes\n", static_cast<unsigned>(frame->len));
        return false;
    }

    for (int y = 0; y < kFrameHeight; ++y)
    {
        const uint8_t *row = frame->buf + static_cast<size_t>(y * kStepY) * stride;
        uint8_t *dst       = buffer + y * kFrameWidth;
        for (int x = 0; x < kFrameWidth; ++x)
        {
            dst[x] = row[static_cast<size_t>(x * kStepX) * bytesPerPixel];
        }
    }
    return true;
}

camera_fb_t *Camera::waitFrame(uint32_t timeoutMs)
{
    const uint32_t start = millis();
    // esp_camera_fb_get 在没有帧时会永久阻塞，先确认队列里有帧再取
    while (!esp_camera_available_frames())
    {
        if (millis() - start > timeoutMs)
            return nullptr;
        vTaskDelay(pdMS_TO_TICKS(5));
    }
    return esp_camera_fb_get();
}

bool Camera::lock(uint32_t timeoutMs)
{
    return _lock != nullptr && xSemaphoreTake(_lock, pdMS_TO_TICKS(timeoutMs)) == pdTRUE;
}

void Camera::unlock()
{
    if (_lock != nullptr)
        xSemaphoreGive(_lock);
}
