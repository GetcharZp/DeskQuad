// display.h 封装 OLED 显示
//
// 绘制走 IDisplay 接口，渲染层因此可以在没有硬件的情况下做单元测试。

#pragma once

#include <Arduino.h>
#include <U8g2lib.h>

// IDisplay 是 OLED 的绘制接口
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

    // FirstPage / NextPage 是 U8g2 的分页绘制协议，见 RenderPage 辅助函数
    virtual void FirstPage() = 0;
    virtual bool NextPage()  = 0;

    // Clear 清空显示缓冲
    virtual void Clear() = 0;

    virtual void SetFont(const uint8_t *font)                                = 0;
    virtual void DrawXBMP(int x, int y, int w, int h, const uint8_t *bitmap) = 0;
    virtual void DrawStr(int x, int y, const char *text)                     = 0;
    virtual void SetCursor(int x, int y)                                     = 0;
    virtual void Print(const char *text)                                     = 0;
};

// U8g2Display 是 IDisplay 基于 SSD1306 硬件 I2C 的实现
class U8g2Display : public IDisplay
{
public:
    U8g2Display();

    void Begin() override;
    void SetRotation(const u8g2_cb_t *rotation) override;
    void FirstPage() override;
    bool NextPage() override;
    void Clear() override;
    void SetFont(const uint8_t *font) override;
    void DrawXBMP(int x, int y, int w, int h, const uint8_t *bitmap) override;
    void DrawStr(int x, int y, const char *text) override;
    void SetCursor(int x, int y) override;
    void Print(const char *text) override;

private:
    U8G2_SSD1306_128X64_NONAME_1_HW_I2C _u8g2;
};
