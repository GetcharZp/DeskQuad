#include <deskquad/render/interaction_overlay.h>

#include <math.h>

namespace
{

    constexpr int kPadX       = 7;  // 胶囊内左右留白
    constexpr int kIconW      = 12; // 图标区域宽度
    constexpr int kIconGap    = 4;  // 图标与文字的间距
    // 高度压到 11px：天气页右侧第一行文字从 y≈12 开始，胶囊不能压上去
    constexpr int kChipH      = 11;
    constexpr int kChipY      = 0;
    constexpr int kChipRight  = 127;

    int clampInt(int value, int lo, int hi)
    {
        if (value < lo)
            return lo;
        if (value > hi)
            return hi;
        return value;
    }

    const char *labelFor(InteractionKind kind)
    {
        switch (kind)
        {
        case InteractionKind::Listening:
            return "ASR";
        case InteractionKind::Thinking:
            return "AI";
        case InteractionKind::Speaking:
            return "TTS";
        default:
            return "";
        }
    }

    // drawBars 画一组随音频能量跳动的电平条
    void drawBars(IDisplay &display, int centerX, int bottomY, uint8_t level, float timeSec)
    {
        const float energy = 0.25f + 0.75f * (level > 100 ? 1.0f : static_cast<float>(level) / 100.0f);

        for (int i = 0; i < 3; i++)
        {
            const float wobble = 0.55f + 0.45f * fabsf(sinf(timeSec * 9.0f + static_cast<float>(i) * 2.1f));
            const int barH     = clampInt(2 + static_cast<int>(4.0f * energy * wobble), 2, 6);
            const int barX     = centerX - 5 + i * 4;
            display.DrawBox(barX, bottomY - barH, 2, barH);
        }
    }

} // namespace

void InteractionOverlay::Paint(IDisplay &display, const InteractionStatus &status, uint32_t nowMs) const
{
    if (status.kind == InteractionKind::None)
        return;

    const char *label = labelFor(status.kind);
    display.SetFont(u8g2_font_6x10_tr);
    const int textW = display.StrWidth(label);

    const int chipW    = kPadX + kIconW + kIconGap + textW + kPadX;
    const int chipX    = kChipRight - chipW;
    const int iconCx   = chipX + kPadX + kIconW / 2;
    const int baseline = kChipY + kChipH - 3;

    // 先用黑底挖空一块，避免胶囊压在表情或文字上时糊成一团
    display.SetDrawColor(0);
    display.DrawRBox(chipX, kChipY, chipW, kChipH, 5);
    display.SetDrawColor(1);
    display.DrawRFrame(chipX, kChipY, chipW, kChipH, 5);

    const float timeSec = static_cast<float>(nowMs) / 1000.0f;

    if (status.kind == InteractionKind::Thinking)
    {
        // 沿小圆环追赶的彗星点，避免 DrawArc 跨 0° 时的方向跳变
        constexpr int kDots = 8;
        const int centerY    = kChipY + kChipH / 2;
        const int head       = static_cast<int>(timeSec * 5.0f) % kDots;

        for (int k = 0; k < 3; k++)
        {
            const int idx     = (head + k) % kDots;
            const float angle = static_cast<float>(idx) * (2.0f * 3.14159265f / kDots) - 1.5707963f;
            const int px      = iconCx + static_cast<int>(lroundf(4.0f * cosf(angle)));
            const int py      = centerY + static_cast<int>(lroundf(4.0f * sinf(angle)));
            if (k == 0)
                display.DrawDisc(px, py, 1);
            else
                display.DrawPixel(px, py);
        }
    }
    else
    {
        drawBars(display, iconCx, baseline, status.level, timeSec);
    }

    display.DrawStr(chipX + kPadX + kIconW + kIconGap, baseline, label);
}
