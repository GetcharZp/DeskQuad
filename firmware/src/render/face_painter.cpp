#include <deskquad/render/face_painter.h>

#include <math.h>

namespace
{
    constexpr int kEyeLeftX  = 42;
    constexpr int kEyeRightX = 86;
    constexpr int kEyeY      = 27;
    constexpr int kMouthX    = 64;
    constexpr int kMouthY    = 47;

    // 小精灵位图，XBM 格式：每行 1 字节，bit0 是最左像素
    const uint8_t kHeart[] PROGMEM = {0x36, 0x7F, 0x7F, 0x3E, 0x1C, 0x08};
    const uint8_t kStar[] PROGMEM  = {0x08, 0x08, 0x3E, 0x1C, 0x36, 0x22};
    const uint8_t kTear[] PROGMEM  = {0x04, 0x04, 0x0E, 0x0E, 0x1F, 0x0E, 0x04};
    const uint8_t kSweat[] PROGMEM = {0x04, 0x0E, 0x1F, 0x1F, 0x0E, 0x04};

    int clampInt(int value, int lo, int hi)
    {
        if (value < lo)
            return lo;
        if (value > hi)
            return hi;
        return value;
    }

    // drawSprite 以左上角为锚点绘制精灵，自动裁剪到屏幕内
    void drawSprite(IDisplay &display, int x, int y, int w, int h, const uint8_t *data)
    {
        const int sx = clampInt(x, 0, 128 - w);
        const int sy = clampInt(y, 0, 64 - h);
        display.DrawXBMP(sx, sy, w, h, data);
    }

    void drawSpriteCentered(IDisplay &display, int x, int y, int w, int h, const uint8_t *data)
    {
        drawSprite(display, x - w / 2, y - h / 2, w, h, data);
    }

    // drawThickLine 用两条错开的线模拟 2px 笔画
    void drawThickLine(IDisplay &display, int x0, int y0, int x1, int y1)
    {
        display.DrawLine(x0, y0, x1, y1);
        display.DrawLine(x0, y0 + 1, x1, y1 + 1);
    }

    void drawBrows(IDisplay &display, const FaceParams &params, int dx, int dy)
    {
        const int baseY = kEyeY - 15 - static_cast<int>(params.browLift * 4.0f) + dy;
        const int tilt  = static_cast<int>(params.browAngle * 4.0f);

        // 左眉：外端在左，外高内低表示生气，内高外低表示难过
        drawThickLine(display, kEyeLeftX - 8 + dx, baseY + tilt, kEyeLeftX + 8 + dx, baseY - tilt);
        drawThickLine(display, kEyeRightX - 8 + dx, baseY - tilt, kEyeRightX + 8 + dx, baseY + tilt);
    }

    void drawEye(IDisplay &display, int cx, int cy, const FaceParams &params, int dx, int dy)
    {
        const int x    = cx + dx;
        const int y    = cy + dy;
        const float open = params.eyeOpen;

        if (params.eyeCross > 0.55f)
        {
            // × 眼：眩晕
            drawThickLine(display, x - 9, y - 9, x + 9, y + 9);
            drawThickLine(display, x + 9, y - 9, x - 9, y + 9);
            return;
        }

        if (open < 0.18f)
        {
            // 闭眼：一条圆头短线
            display.DrawRBox(x - 11, y - 2, 22, 4, 2);
            return;
        }

        if (params.eyeRound > 0.62f && open > 0.45f)
        {
            // 弯月笑眼：三层同心上半圆弧
            const int radius = 10;
            display.DrawArc(x, y + 4, radius, 0, 128);
            display.DrawArc(x, y + 4, radius - 1, 0, 128);
            display.DrawArc(x, y + 4, radius - 2, 0, 128);
            return;
        }

        // 圆眼：白色眼白 + 黑色瞳孔 + 白色高光
        const int rx = 12;
        const int ry = clampInt(static_cast<int>(13.0f * open), 2, 13);
        display.DrawFilledEllipse(x, y, rx, ry);

        // 眼白太扁时画瞳孔会挖穿眼白，直接跳过
        if (open < 0.32f || ry < 4)
            return;

        int pupilR = clampInt(static_cast<int>(5.5f * params.pupilSize), 2, 7);
        if (pupilR > ry - 1)
            pupilR = ry - 1;
        const int maxOffX = (rx - pupilR - 1) > 0 ? (rx - pupilR - 1) : 0;
        const int maxOffY = (ry - pupilR - 1) > 0 ? (ry - pupilR - 1) : 0;
        const int px      = x + static_cast<int>(params.pupilX * maxOffX);
        const int py      = y + static_cast<int>(params.pupilY * maxOffY);

        display.SetDrawColor(0);
        display.DrawDisc(px, py, pupilR);
        display.SetDrawColor(1);
        display.DrawDisc(px - 2, py - 2, 1);
    }

    void drawMouth(IDisplay &display, const FaceParams &params, int dx, int dy)
    {
        const int cx = kMouthX + dx;
        const int cy = kMouthY + dy;
        const int w  = static_cast<int>(15.0f * params.mouthWidth);

        if (params.mouthOpen > 0.14f)
        {
            // 张嘴：白色外圈 + 黑色口腔，形成清晰的“O”
            const int ry = 2 + static_cast<int>(10.0f * params.mouthOpen);
            display.DrawFilledEllipse(cx, cy, w, ry);
            display.SetDrawColor(0);
            display.DrawFilledEllipse(cx, cy, w - 2, ry - 2 > 0 ? ry - 2 : 1);
            display.SetDrawColor(1);
            return;
        }

        // 闭合嘴：二次贝塞尔曲线，curve 为正时嘴角上翘
        const float curve = params.mouthCurve;
        const int y0      = cy - static_cast<int>(3.0f * curve);
        const int yc      = cy + static_cast<int>(9.0f * curve);
        const int x0      = cx - w;
        const int x1      = cx + w;

        int prevX = x0;
        int prevY = y0;
        constexpr int kSteps = 12;
        for (int i = 1; i <= kSteps; i++)
        {
            const float t  = static_cast<float>(i) / kSteps;
            const float mt = 1.0f - t;
            const int x    = static_cast<int>(mt * mt * x0 + 2.0f * mt * t * cx + t * t * x1);
            const int y    = static_cast<int>(mt * mt * y0 + 2.0f * mt * t * yc + t * t * y0);
            drawThickLine(display, prevX, prevY, x, y);
            prevX = x;
            prevY = y;
        }
    }

    void drawBlush(IDisplay &display, const FaceParams &params, int dx, int dy)
    {
        if (params.blush <= 0.05f)
            return;

        const int level  = params.blush > 0.66f ? 2 : (params.blush > 0.33f ? 1 : 0);
        const int centers[2] = {26 + dx, 102 + dx};
        const int cy          = 43 + dy;

        for (int c = 0; c < 2; c++)
        {
            for (int y = -3; y <= 3; y++)
            {
                for (int x = -7; x <= 7; x++)
                {
                    // 椭圆内 + 棋盘抖动，抖动密度表示浓度
                    if ((x * x) / 49.0f + (y * y) / 9.0f > 1.0f)
                        continue;
                    const int pattern = (x + y + 16) & 3;
                    if (level == 2 || (level == 1 && pattern < 2) || (level == 0 && pattern == 0))
                        display.DrawPixel(centers[c] + x, cy + y);
                }
            }
        }
    }

    void drawAngerMark(IDisplay &display, int cx, int cy, float timeSec)
    {
        // 脉动的四短线“怒气”符号
        const float pulse = 0.65f + 0.35f * fabsf(sinf(timeSec * 7.0f));
        const int out     = static_cast<int>(6.0f * pulse) + 2;
        const int inner   = 2;

        drawThickLine(display, cx - inner, cy - inner, cx - out, cy - out);
        drawThickLine(display, cx + inner, cy - inner, cx + out, cy - out);
        drawThickLine(display, cx - inner, cy + inner, cx - out, cy + out);
        drawThickLine(display, cx + inner, cy + inner, cx + out, cy + out);
    }

    void drawDecorations(IDisplay &display, const FaceState &state, int dx, int dy)
    {
        switch (state.expression)
        {
        case EmojiId::Angry:
            drawAngerMark(display, 106 + dx, 11 + dy, state.timeSec);
            break;
        case EmojiId::Curious: {
            const int bob = static_cast<int>(2.0f * sinf(state.timeSec * 3.0f));
            display.SetFont(u8g2_font_6x10_tr);
            display.DrawStr(100 + dx, 17 + dy + bob, "?");
            break;
        }
        case EmojiId::Dizzy: {
            // 三颗星绕头顶旋转
            for (int i = 0; i < 3; i++)
            {
                const float angle = state.timeSec * 2.4f + static_cast<float>(i) * 2.094f;
                const int x       = 64 + static_cast<int>(40.0f * cosf(angle)) + dx;
                const int y       = 12 + static_cast<int>(7.0f * sinf(angle)) + dy / 2;
                drawSpriteCentered(display, x, y, 7, 6, kStar);
            }
            break;
        }
        default:
            break;
        }
    }

    void drawParticle(IDisplay &display, const FaceParticle &particle, float timeSec)
    {
        if (particle.life <= 0.0f)
            return;

        const int x = static_cast<int>(particle.x + sinf(particle.phase + timeSec * 3.0f) * 3.0f);
        const int y = static_cast<int>(particle.y);

        switch (particle.kind)
        {
        case ParticleKind::Heart:
            drawSpriteCentered(display, x, y, 7, 6, kHeart);
            break;
        case ParticleKind::Tear:
            drawSpriteCentered(display, x, y, 5, 7, kTear);
            break;
        case ParticleKind::Sweat:
            drawSpriteCentered(display, x, y, 5, 6, kSweat);
            break;
        case ParticleKind::Sparkle: {
            const int r = static_cast<int>(2.0f + 2.0f * particle.scale);
            display.DrawLine(x - r, y, x + r, y);
            display.DrawLine(x, y - r, x, y + r);
            if (r >= 3)
            {
                display.DrawLine(x - 1, y - 1, x + 1, y + 1);
                display.DrawLine(x + 1, y - 1, x - 1, y + 1);
            }
            break;
        }
        }
    }

} // namespace

void FacePainter::Paint(IDisplay &display, const FaceState &state) const
{
    display.SetDrawColor(1);

    const int dx = static_cast<int>(lroundf(state.params.faceX));
    const int dy = static_cast<int>(lroundf(state.params.faceY));

    drawBrows(display, state.params, dx, dy);
    drawEye(display, kEyeLeftX, kEyeY, state.params, dx, dy);
    drawEye(display, kEyeRightX, kEyeY, state.params, dx, dy);
    drawBlush(display, state.params, dx, dy);
    drawMouth(display, state.params, dx, dy);
    drawDecorations(display, state, dx, dy);

    for (uint8_t i = 0; i < state.particleCount; i++)
        drawParticle(display, state.particles[i], state.timeSec);
}
