#include <deskquad/render/expression_renderer.h>

#include <deskquad/render/image_data.h>

namespace
{

    // 动画帧间隔：约 30fps。IDisplay 会跳过没有变化的图块，
    // 因此即使静止画面也按这个节拍调用也不会产生额外 I2C 流量。
    constexpr unsigned long kFrameMs = 33;

    // Logo 是 128×22 的位图，需要垂直居中
    constexpr uint8_t kLogoY = 21;

} // namespace

ExpressionRenderer::ExpressionRenderer(IDisplay &display, const WeatherView &weatherView, const ClockView &clockView)
    : _display(display), _weatherView(weatherView), _clockView(clockView), _current(EmojiId::Happy), _lastFrameMs(0)
{
}

void ExpressionRenderer::Begin()
{
    _display.Clear();
    _face.Begin(millis());
    _face.SetExpression(_current);
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

void ExpressionRenderer::drawBitmapPage(const uint8_t *data, uint8_t width, uint8_t height, uint8_t y) const
{
    _display.FirstPage();
    _display.SetDrawColor(1);
    _display.DrawXBMP(0, y, width, height, data);
    _display.NextPage();
}

void ExpressionRenderer::renderFace()
{
    _display.ClearBuffer();
    _painter.Paint(_display, _face.State());
    _display.SendBuffer();
}

void ExpressionRenderer::Render(const WeatherData &weather, const TimeService &timeService, bool wifiConnected,
                                const FaceInputs &inputs)
{
    // 输入每帧都更新，即使这一帧因为节流不绘制
    _face.SetInputs(inputs);

    const unsigned long now = millis();
    if (now - _lastFrameMs < kFrameMs)
        return;
    _lastFrameMs = now;

    switch (_current)
    {
    case EmojiId::Weather:
        _weatherView.Render(_display, weather, wifiConnected);
        return;
    case EmojiId::Clock:
        _clockView.Render(_display, timeService, wifiConnected);
        return;
    case EmojiId::Logo:
        drawBitmapPage(logo, 128, 22, kLogoY);
        return;
    default:
        break;
    }

    _face.Update(now);
    renderFace();
}
