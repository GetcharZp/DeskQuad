// OV2640 摄像头实现
#include <deskquad/perception/ov2640.h>

#include <Wire.h>
#include <deskquad/perception/gesture_recognizer.h>

namespace
{

    constexpr uint32_t kSpiClockHz       = 4000000;
    constexpr uint32_t kCaptureTimeoutMs = 300;

    // 传感器输出尺寸。InitCAM 默认给 QVGA (320×240)，再缩到 160×120 把
    // FIFO 传输量减半；一帧就是 160×120 个 RGB565 像素。
    constexpr int kSensorWidth     = 160;
    constexpr int kSensorHeight    = 120;
    constexpr uint32_t kFrameBytes = kSensorWidth * kSensorHeight * 2;

    constexpr int kRowBytes    = kSensorWidth * 2;
    constexpr int kFrameWidth  = deskquad::kGestureFrameWidth;
    constexpr int kFrameHeight = deskquad::kGestureFrameHeight;
    constexpr int kStepX       = kSensorWidth / kFrameWidth;
    constexpr int kStepY       = kSensorHeight / kFrameHeight;

    static_assert(kSensorWidth % kFrameWidth == 0 && kSensorHeight % kFrameHeight == 0,
                  "灰度帧尺寸必须整除传感器输出尺寸");

    // rgb565ToGray 把一个大端 RGB565 像素转成亮度
    //
    // 权重是 0.299/0.587/0.114 按 5/6/5 位宽放大 32 倍后的整数形式，
    // 省去先把分量扩展到 8 位的步骤。
    uint8_t rgb565ToGray(uint8_t high, uint8_t low)
    {
        const uint16_t pixel = (static_cast<uint16_t>(high) << 8) | low;
        const uint16_t r     = (pixel >> 11) & 0x1F;
        const uint16_t g     = (pixel >> 5) & 0x3F;
        const uint16_t b     = pixel & 0x1F;
        return static_cast<uint8_t>((r * 79 + g * 76 + b * 30 + 16) >> 5);
    }

} // namespace

bool Camera::Begin()
{
    // ESP32-S3 的 SPI/I2C 可以路由到任意 GPIO，显式指定以避免占用舵机引脚
    SPI.begin(deskquad::kCameraSckPin, deskquad::kCameraMisoPin, deskquad::kCameraMosiPin, -1);
    SPI.setFrequency(kSpiClockHz);
    Wire.begin(deskquad::kCameraSdaPin, deskquad::kCameraSclPin);

    // 复位 ArduCAM 的 FIFO 控制逻辑
    _cam.write_reg(0x07, 0x80);
    delay(10);
    _cam.write_reg(0x07, 0x00);
    delay(10);

    // 先读传感器 ID：没接模块时就不必再跑一遍几百条寄存器配置
    uint8_t vid = 0;
    uint8_t pid = 0;
    _cam.wrSensorReg8_8(0xFF, 0x01);
    _cam.rdSensorReg8_8(OV2640_CHIPID_HIGH, &vid);
    _cam.rdSensorReg8_8(OV2640_CHIPID_LOW, &pid);
    if (vid != 0x26 || (pid != 0x41 && pid != 0x42))
    {
        Serial.printf("[warn] camera: sensor not found (VID=0x%02X PID=0x%02X)\n", vid, pid);
        return false;
    }

    // BMP 让 InitCAM 走非 JPEG 分支：OV2640_QVGA 配置成 RGB565 输出；
    // 再套用 160×120 的缩放表，FIFO 每帧正好是 kFrameBytes 字节。
    _cam.set_format(BMP);
    _cam.InitCAM();
    delay(100);
    _cam.OV2640_set_JPEG_size(OV2640_160x120);
    delay(50);

    _available = true;
    return true;
}

bool Camera::Capture(uint8_t *buffer)
{
    if (!_available || buffer == nullptr)
        return false;

    _cam.flush_fifo();
    _cam.start_capture();
    if (!waitCaptureDone())
        return false;

    const uint32_t length = _cam.read_fifo_length();
    if (length != kFrameBytes)
    {
        Serial.printf("[warn] camera: FIFO length %lu, expected %lu\n", static_cast<unsigned long>(length),
                      static_cast<unsigned long>(kFrameBytes));
        return false;
    }

    readFrame(buffer);
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

bool Camera::waitCaptureDone()
{
    const uint32_t start = millis();
    while (!_cam.get_bit(ARDUCHIP_TRIG, CAP_DONE_MASK))
    {
        if (millis() - start > kCaptureTimeoutMs)
        {
            Serial.println("[warn] camera: capture timeout");
            return false;
        }
        delay(1);
    }
    return true;
}

void Camera::readFrame(uint8_t *buffer)
{
    uint8_t row[kRowBytes];

    // 整帧走一次 burst 读，比逐字节 read_fifo 少几万次 SPI 事务
    _cam.CS_LOW();
    _cam.set_fifo_burst();

    for (int y = 0; y < kSensorHeight; ++y)
    {
        SPI.transferBytes(nullptr, row, sizeof(row));
        if (y % kStepY != 0)
            continue;

        uint8_t *dst = buffer + (y / kStepY) * kFrameWidth;
        for (int x = 0; x < kFrameWidth; ++x)
        {
            const uint8_t *px = row + x * kStepX * 2;
            dst[x]            = rgb565ToGray(px[0], px[1]);
        }
    }

    _cam.CS_HIGH();
}
