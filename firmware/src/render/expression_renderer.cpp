#include <deskquad/render/expression_renderer.h>

#include <deskquad/render/image_data.h>

namespace
{

    // EmojiBitmap 描述一个表情页使用的位图
    struct EmojiBitmap
    {
        const uint8_t *data; // 位图数据，为空表示该页由 view 绘制
        uint8_t width;
        uint8_t height;
        uint8_t y; // 顶部纵坐标，Logo 需要垂直居中
    };

    const EmojiBitmap kEmojiBitmaps[] = {
        {happy, 128, 64, 0},   // Happy
        {angry, 128, 64, 0},   // Angry
        {error, 128, 64, 0},   // Error
        {curious, 128, 64, 0}, // Curious
        {love, 128, 64, 0},    // Love
        {sad, 128, 64, 0},     // Sad
        {dizzy, 128, 64, 0},   // Dizzy
        {nullptr, 0, 0, 0},    // Weather，由 WeatherView 绘制
        {nullptr, 0, 0, 0},    // Clock，由 ClockView 绘制
        {logo, 128, 22, 21},   // Logo
    };

    constexpr uint8_t kEmojiCount = sizeof(kEmojiBitmaps) / sizeof(kEmojiBitmaps[0]);
    static_assert(kEmojiCount == static_cast<uint8_t>(EmojiId::Logo) + 1, "位图表必须覆盖全部 EmojiId");

    // 两次重绘的最小间隔；页面内容不会逐帧变化，没必要每个主循环都刷屏
    constexpr unsigned long kRefreshMs = 100;

} // namespace

ExpressionRenderer::ExpressionRenderer(IDisplay &display, const WeatherView &weatherView, const ClockView &clockView)
    : _display(display), _weatherView(weatherView), _clockView(clockView), _current(EmojiId::Happy),
      _prev(EmojiId::Happy), _lastRenderMs(0)
{
}

void ExpressionRenderer::Begin()
{
    _display.Clear();
    _prev         = _current;
    _lastRenderMs = millis();
}

void ExpressionRenderer::drawBitmapPage(const uint8_t *data, uint8_t width, uint8_t height, uint8_t y) const
{
    _display.FirstPage();
    do
    {
        _display.DrawXBMP(0, y, width, height, data);
    } while (_display.NextPage());
}

void ExpressionRenderer::Render(const WeatherData &weather, const TimeService &timeService, bool wifiConnected)
{
    const unsigned long now = millis();
    if (_current == _prev && now - _lastRenderMs < kRefreshMs)
        return;
    _lastRenderMs = now;

    // 原实现只在表情切换时清屏，其余时候直接重绘覆盖
    if (_current != _prev)
    {
        _display.Clear();
        _prev = _current;
    }

    const uint8_t index = static_cast<uint8_t>(_current);
    switch (_current)
    {
    case EmojiId::Weather:
        _weatherView.Render(_display, weather, wifiConnected);
        break;
    case EmojiId::Clock:
        _clockView.Render(_display, timeService, wifiConnected);
        break;
    default: {
        const EmojiBitmap &bitmap = kEmojiBitmaps[index];
        if (bitmap.data != nullptr)
        {
            drawBitmapPage(bitmap.data, bitmap.width, bitmap.height, bitmap.y);
        }
        break;
    }
    }
}
