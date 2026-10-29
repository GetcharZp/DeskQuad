#include <deskquad/render/views.h>

#include <deskquad/render/image_data.h>

namespace
{

    // 三个页面共用的绘制循环：U8g2 每画一页都要重放全部绘制指令
    template <typename DrawFn> void renderPage(IDisplay &display, DrawFn draw)
    {
        display.FirstPage();
        do
        {
            draw();
        } while (display.NextPage());
    }

    const char *kTempUnit = " C";
    // 晴天的温度单位沿用上游写法，为 "%" 而非 " C"
    const char *kSunnyTempUnit = " %";

} // namespace

void NetworkView::Render(IDisplay &display) const
{
    renderPage(display, [&display]() {
        display.SetFont(u8g2_font_ncenB08_tr);
        display.DrawXBMP(0, 0, 64, 64, wifi);
        display.DrawStr(64, 20, "IP:");
        display.DrawStr(64, 40, "192.168.4.1");
        display.DrawStr(64, 60, "Need NET");
    });
}

void WeatherView::Render(IDisplay &display, const WeatherData &data, bool wifiConnected) const
{
    if (!wifiConnected)
    {
        NetworkView().Render(display);
        return;
    }

    const bool rainy =
        data.weather == "小雨" || data.weather == "大雨" || data.weather == "暴雨" || data.weather == "雨";
    const bool sunny = data.weather == "晴";

    const uint8_t *icon = cloud;
    const char *unit    = kTempUnit;
    if (rainy)
    {
        icon = rain;
    }
    else if (sunny)
    {
        icon = sun;
        unit = kSunnyTempUnit;
    }

    renderPage(display, [&display, &data, icon, unit]() {
        display.SetFont(u8g2_font_ncenB08_tr);
        display.DrawXBMP(0, 0, 64, 64, icon);
        display.DrawStr(64, 20, "Temp");
        display.DrawStr(64, 30, (data.temperature + unit).c_str());
        display.DrawStr(64, 50, "Humidity");
        display.DrawStr(64, 60, (data.humidity + " %").c_str());
    });
}

void ClockView::Render(IDisplay &display, const TimeService &timeService, bool wifiConnected) const
{
    if (!wifiConnected)
    {
        NetworkView().Render(display);
        return;
    }

    char timeText[8];
    timeService.Format(timeText, sizeof(timeText));

    renderPage(display, [&display, timeText]() {
        display.SetFont(u8g2_font_ncenB14_tr);
        display.DrawXBMP(0, 0, 64, 64, clockIcon);
        display.DrawStr(64, 30, "TIME");
        display.SetCursor(64, 50);
        display.Print(timeText);
    });
}
