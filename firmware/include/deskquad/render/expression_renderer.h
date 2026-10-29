// expression_renderer.h 按表情编号绘制 OLED
//
// 表情 0-6 与 9 是位图页，7 与 8 分别是天气页和时间页，交由对应的
// view 绘制。原实现把十一个分支抄了十一段几乎相同的绘制代码，
// 这里改为表驱动。

#pragma once

#include <deskquad/core/states.h>
#include <deskquad/hal/display.h>
#include <deskquad/hal/time_service.h>
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

    // Begin 清空屏幕
    void Begin();

    // SetEmoji 切换当前表情页
    void SetEmoji(EmojiId id) { _current = id; }

    // Current 返回当前表情页
    EmojiId Current() const { return _current; }

    // Render 绘制当前页面
    //
    // # Params:
    //
    //	weather: 天气数据
    //	timeService: 时间来源
    //	wifiConnected: 未联网时天气页与时间页改画断网提示
    void Render(const WeatherData &weather, const TimeService &timeService, bool wifiConnected);

private:
    void drawBitmapPage(const uint8_t *data, uint8_t width, uint8_t height, uint8_t y) const;

    IDisplay &_display;
    const WeatherView &_weatherView;
    const ClockView &_clockView;
    EmojiId _current;
    EmojiId _prev;
    unsigned long _lastRenderMs;
};
