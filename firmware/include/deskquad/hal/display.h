// display.h 封装 OLED 显示
//
// 绘制走 IDisplay 接口，渲染层因此可以在没有硬件的情况下做单元测试。
// 驱动使用 U8g2 全缓冲模式（_F_）：绘制先写 1KB 本地缓冲，SendBuffer
// 时与上一帧比较，只有内容变化才真正走 I2C，动画帧因此可以做到 30fps
// 而不必为静止画面反复刷屏。

#pragma once

#include <Arduino.h>
#include <U8g2lib.h>

// IDisplay 是 OLED 的绘制接口
//
// 坐标原点在左上角，颜色 0 表示擦除像素、1 表示点亮，仅全缓冲模式支持
// 颜色 0 的“挖空”绘制（瞳孔、口腔都依赖它）。
class IDisplay
{
public:
    virtual ~IDisplay() {}

    // Begin 初始化屏幕
    virtual void Begin() = 0;

    // SetRotation 设置显示方向
    //
    // # Params:
    //
    //	rotation: 方向常量，如 U8G2_R0、U8G2_R2
    virtual void SetRotation(const u8g2_cb_t *rotation) = 0;

    // FirstPage / NextPage 是 U8g2 风格的分页绘制协议
    //
    // 全缓冲模式下 FirstPage 清空缓冲，NextPage 提交一帧并返回 false，
    // 调用方写完一次即可退出 do/while 循环。
    virtual void FirstPage() = 0;
    virtual bool NextPage()  = 0;

    // ClearBuffer / SendBuffer 是显式的缓冲控制
    virtual void ClearBuffer() = 0;
    virtual void SendBuffer()  = 0;

    // Clear 只清空本地缓冲，下一次 SendBuffer 才提交
    virtual void Clear() = 0;

    virtual void SetFont(const uint8_t *font) = 0;
    virtual void SetDrawColor(uint8_t color)  = 0;

    // 文本
    virtual void DrawStr(int x, int y, const char *text) = 0;
    virtual void SetCursor(int x, int y)                 = 0;
    virtual void Print(const char *text)                 = 0;

    // StrWidth 返回当前字体下文本的像素宽度，用于居中排版
    virtual int StrWidth(const char *text) = 0;

    // 位图
    virtual void DrawXBMP(int x, int y, int w, int h, const uint8_t *bitmap) = 0;

    // 图元
    virtual void DrawPixel(int x, int y)                                   = 0;
    virtual void DrawLine(int x0, int y0, int x1, int y1)                  = 0;
    virtual void DrawBox(int x, int y, int w, int h)                       = 0;
    virtual void DrawRBox(int x, int y, int w, int h, int r)               = 0;
    virtual void DrawFrame(int x, int y, int w, int h)                     = 0;
    virtual void DrawRFrame(int x, int y, int w, int h, int r)             = 0;
    virtual void DrawCircle(int x, int y, int r)                           = 0;
    virtual void DrawDisc(int x, int y, int r)                             = 0;
    virtual void DrawFilledEllipse(int x, int y, int rx, int ry)           = 0;
    virtual void DrawArc(int x, int y, int r, uint8_t start, uint8_t end)  = 0;
};

// U8g2Display 是 IDisplay 基于 SSD1306 硬件 I2C 的全缓冲实现
class U8g2Display : public IDisplay
{
public:
    U8g2Display();

    void Begin() override;
    void SetRotation(const u8g2_cb_t *rotation) override;

    void FirstPage() override;
    bool NextPage() override;
    void ClearBuffer() override;
    void SendBuffer() override;
    void Clear() override;

    void SetFont(const uint8_t *font) override;
    void SetDrawColor(uint8_t color) override;
    void DrawStr(int x, int y, const char *text) override;
    void SetCursor(int x, int y) override;
    void Print(const char *text) override;
    int StrWidth(const char *text) override;
    void DrawXBMP(int x, int y, int w, int h, const uint8_t *bitmap) override;

    void DrawPixel(int x, int y) override;
    void DrawLine(int x0, int y0, int x1, int y1) override;
    void DrawBox(int x, int y, int w, int h) override;
    void DrawRBox(int x, int y, int w, int h, int r) override;
    void DrawFrame(int x, int y, int w, int h) override;
    void DrawRFrame(int x, int y, int w, int h, int r) override;
    void DrawCircle(int x, int y, int r) override;
    void DrawDisc(int x, int y, int r) override;
    void DrawFilledEllipse(int x, int y, int rx, int ry) override;
    void DrawArc(int x, int y, int r, uint8_t start, uint8_t end) override;

private:
    U8G2_SSD1306_128X64_NONAME_F_HW_I2C _u8g2;

    // _sent 保存上一次真正发到屏幕的画面，用于跳过相同帧
    uint8_t _sent[128 * 64 / 8];
    bool _sentValid;
};
