// expression_renderer.h 按表情编号绘制 OLED
//
// 表情 0-6 由 FaceAnimator + FacePainter 程序化绘制（参数化脸 + 特效），
// 7 与 8 分别是天气页和时间页，9 是开机 Logo。页面切换在这里统一处理。
//
// 表情页以 ~30fps 刷新；页面类（天气/时间/Logo）同样走这条时钟，但
// IDisplay::SendBuffer 会跳过没有变化的图块，静止画面不会产生 I2C 流量。

#pragma once

#include <deskquad/core/states.h>
#include <deskquad/hal/display.h>
#include <deskquad/hal/time_service.h>
#include <deskquad/render/face_model.h>
#include <deskquad/render/face_painter.h>
#include <deskquad/render/views.h>

// ExpressionRenderer 绘制 OLED 的当前页面
class ExpressionRenderer
{
public:
    // 构造函数
    //
    // # Params:
    //
    //	display: 显示接口
    //	weatherView: 天气页
    //	clockView: 时间页
    ExpressionRenderer(IDisplay &display, const WeatherView &weatherView, const ClockView &clockView);

    // Begin 复位显示与表情动画
    void Begin();

    // SetEmoji 切换当前表情页
    void SetEmoji(EmojiId id);

    // Current 返回当前表情页
    EmojiId Current() const { return _current; }

    // Render 绘制当前页面
    //
    // # Params:
    //
    //	weather: 天气数据
    //	timeService: 时间来源
    //	wifiConnected: 未联网时天气页与时间页改画断网提示
    //	inputs: PAD 情感、扬声器音符与交互状态，驱动表情动画
    void Render(const WeatherData &weather, const TimeService &timeService, bool wifiConnected,
                const FaceInputs &inputs);

private:
    void renderFace();
    void drawBitmapPage(const uint8_t *data, uint8_t width, uint8_t height, uint8_t y) const;

    IDisplay &_display;
    const WeatherView &_weatherView;
    const ClockView &_clockView;

    FaceAnimator _face;
    FacePainter _painter;

    EmojiId _current;
    unsigned long _lastFrameMs;
};
