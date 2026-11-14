#include <deskquad/render/views.h>

#include <stdio.h>

#include <deskquad/render/image_data.h>

namespace
{

    const char *kTempUnit = " C";
    // 晴天的温度单位沿用上游写法，为 "%" 而非 " C"
    const char *kSunnyTempUnit = " %";

    const char *kWeekdays[7] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};

    // 时间页排版：顶部日期、中部大号时间、右下角秒数、底部秒进度轨
    constexpr int kTimeBaseline = 44;
    constexpr int kRailX        = 4;
    constexpr int kRailY        = 58;
    constexpr int kRailW        = 120;
    constexpr int kRailFillW    = kRailW - 2;

    // drawBigTime 把小时与分钟拆成两段，冒号单独绘制以便做闪烁
    void drawBigTime(IDisplay &display, const ClockReading &now)
    {
        if (!now.synced)
        {
            // _tn 字体只有数字与冒号，提示文字换成带字母的 logisoso
            display.SetFont(u8g2_font_logisoso24_tr);
            const char *waiting = "SYNC";
            const int width     = display.StrWidth(waiting);
            display.DrawStr((128 - width) / 2, kTimeBaseline, waiting);
            return;
        }

        display.SetFont(u8g2_font_logisoso30_tn);

        char hours[3];
        char minutes[3];
        snprintf(hours, sizeof(hours), "%02u", static_cast<unsigned>(now.hour));
        snprintf(minutes, sizeof(minutes), "%02u", static_cast<unsigned>(now.minute));

        // 冒号用两个圆点自己画，比字体里的冒号更好控制粗细与节奏
        const int colonWidth  = 3;
        const int gap         = 3;
        const int hourWidth   = display.StrWidth(hours);
        const int minuteWidth = display.StrWidth(minutes);
        const int total       = hourWidth + gap + colonWidth + gap + minuteWidth;

        int x = (128 - total) / 2;
        display.DrawStr(x, kTimeBaseline, hours);
        x += hourWidth + gap;

        // 冒号每秒亮灭一次，让整块静态画面保持一点呼吸感
        if ((now.second & 1u) == 0u)
        {
            display.DrawDisc(x + 1, kTimeBaseline - 20, 1);
            display.DrawDisc(x + 1, kTimeBaseline - 10, 1);
        }
        x += colonWidth + gap;

        display.DrawStr(x, kTimeBaseline, minutes);
    }

    void drawSecondsRail(IDisplay &display, const ClockReading &now)
    {
        display.SetFont(u8g2_font_6x10_tr);

        char seconds[3];
        if (now.synced)
            snprintf(seconds, sizeof(seconds), "%02u", static_cast<unsigned>(now.second));
        else
            snprintf(seconds, sizeof(seconds), "--");

        const int width = display.StrWidth(seconds);
        display.DrawStr(128 - 4 - width, 54, seconds);

        display.DrawFrame(kRailX, kRailY, kRailW, 3);
        if (now.synced && now.second > 0)
        {
            const int fill = kRailFillW * now.second / 59;
            display.DrawBox(kRailX + 1, kRailY + 1, fill, 1);
        }
    }

} // namespace

void NetworkView::Draw(IDisplay &display) const
{
    display.SetDrawColor(1);
    display.SetFont(u8g2_font_ncenB08_tr);
    display.DrawXBMP(0, 0, 64, 64, wifi);
    display.DrawStr(64, 20, "IP:");
    display.DrawStr(64, 40, "192.168.4.1");
    display.DrawStr(64, 60, "Need NET");
}

void WeatherView::Draw(IDisplay &display, const WeatherData &data, bool wifiConnected) const
{
    if (!wifiConnected)
    {
        NetworkView().Draw(display);
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

    display.SetDrawColor(1);
    display.SetFont(u8g2_font_ncenB08_tr);
    display.DrawXBMP(0, 0, 64, 64, icon);
    display.DrawStr(64, 20, "Temp");
    display.DrawStr(64, 30, (data.temperature + unit).c_str());
    display.DrawStr(64, 50, "Humidity");
    display.DrawStr(64, 60, (data.humidity + " %").c_str());
}

void ClockView::Draw(IDisplay &display, const TimeService &timeService, bool wifiConnected) const
{
    if (!wifiConnected)
    {
        NetworkView().Draw(display);
        return;
    }

    const ClockReading now = timeService.Now();

    // 顶部：星期 + 日期，作为大号时间的脚注
    char date[16];
    if (now.synced)
    {
        const char *weekday = kWeekdays[now.weekday % 7];
        snprintf(date, sizeof(date), "%s %02u-%02u", weekday, static_cast<unsigned>(now.month),
                 static_cast<unsigned>(now.day));
    }
    else
    {
        snprintf(date, sizeof(date), "WAIT NTP");
    }

    display.SetDrawColor(1);
    display.SetFont(u8g2_font_6x10_tr);
    display.DrawStr(4, 9, date);

    drawBigTime(display, now);
    drawSecondsRail(display, now);
}
