#include <deskquad/render/expression_renderer.h>

#include <deskquad/render/image_data.h>

namespace
{

    // 动画帧间隔：约 30fps。IDisplay 会跳过没有变化的图块，
    // 因此即使静止画面也按这个节拍调用也不会产生额外 I2C 流量。
    constexpr unsigned long kFrameMs = 33;

} // namespace

ExpressionRenderer::ExpressionRenderer(IDisplay &display, const WeatherView &weatherView, const ClockView &clockView)
    : _display(display), _weatherView(weatherView), _clockView(clockView), _current(EmojiId::Clock), _lastFrameMs(0)
{
}

void ExpressionRenderer::Begin()
{
    _display.Clear();
    _face.Begin(millis());
    _lastFrameMs = millis() - kFrameMs;
}

void ExpressionRenderer::SetEmoji(EmojiId id)
{
    if (id == _current)
        return;

    _current = id;
    if (FaceAnimator::IsFacePage(id))
        _face.SetExpression(id);
}

void ExpressionRenderer::Render(const WeatherData &weather, const TimeService &timeService, bool wifiConnected,
                                const FaceInputs &inputs, const InteractionStatus &status, bool force)
{
    // 输入每帧都更新，即使这一帧因为节流不绘制
    _face.SetInputs(inputs);

    const unsigned long now = millis();
    if (!force && now - _lastFrameMs < kFrameMs)
        return;
    _lastFrameMs = now;

    // Logo 是全屏位图，不叠加任何状态提示
    if (_current == EmojiId::Logo)
    {
        _display.FirstPage();
        do
        {
            _display.SetDrawColor(1);
            _display.DrawXBMP(0, 0, 128, 64, logo);
        } while (_display.NextPage());
        return;
    }

    _display.ClearBuffer();

    switch (_current)
    {
    case EmojiId::Weather:
        _weatherView.Draw(_display, weather, wifiConnected);
        break;
    case EmojiId::Clock:
        _clockView.Draw(_display, timeService, wifiConnected);
        break;
    default:
        _face.Update(now);
        _painter.Paint(_display, _face.State());
        break;
    }

    _overlay.Paint(_display, status, static_cast<uint32_t>(now));
    _display.SendBuffer();
}
