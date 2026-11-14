#include <deskquad/render/face_painter.h>

#include <math.h>

namespace
{
    // 整张脸只有两只眼睛：位置、大小、倾角、眼皮、瞳孔全都参与表演，
    // 不再画眉毛、嘴、腮红或粒子特效。
    constexpr int kEyeLeftX  = 38;
    constexpr int kEyeRightX = 90;
    constexpr int kEyeY      = 33;
    constexpr float kTiltMax = 0.24f; // eyeTilt = ±1 时约 ±14°

    int clampInt(int value, int lo, int hi)
    {
        if (value < lo)
            return lo;
        if (value > hi)
            return hi;
        return value;
    }

    float clampf(float value, float lo, float hi)
    {
        if (value < lo)
            return lo;
        if (value > hi)
            return hi;
        return value;
    }

    // EyeGeom 是一只眼睛在局部坐标系里的形状
    struct EyeGeom
    {
        float w;      // 半宽
        float h;      // 半高
        float invW;
        float invH;
        float cosA;
        float sinA;
        float slant;  // 上眼睑倾角
        float squint; // 下眼睑上抬
        float sign;   // 左眼 +1，右眼 -1（把“内端”统一到 +u）
    };

    // insideEye 判断局部坐标 (dx, dy) 是否落在眼白内
    //
    // 眼型是 |u/w|³ + |v/h|³ ≤ 1 的超椭圆，比正圆更像屏幕上的圆角眼；
    // 上眼睑按倾角切一刀，下眼睑按 squint 做一条上抬的抛物线。
    bool insideEye(const EyeGeom &geom, float dx, float dy)
    {
        const float u = geom.cosA * dx + geom.sinA * dy;
        const float v = -geom.sinA * dx + geom.cosA * dy;

        const float ax = fabsf(u) * geom.invW;
        const float ay = fabsf(v) * geom.invH;
        if (ax * ax * ax + ay * ay * ay > 1.0f)
            return false;

        const float lx  = u * geom.invW;
        const float top = -geom.h + geom.slant * geom.h * 0.9f * lx * geom.sign;
        if (v < top)
            return false;

        const float bottom = geom.h - geom.squint * geom.h * (1.0f - lx * lx);
        return v <= bottom;
    }

    // fillHeart 用“两个圆瓣 + 下方收尖”画心形，size 为半宽
    //
    // 隐函数心形在 20 多像素宽时缺口只有 1~2px，顶部还会超出采样范围，
    // 看起来像“上半矩形 + 下半尖角”；这里改用圆瓣和楔形拼出清晰的 V 形缺口。
    void fillHeart(IDisplay &display, int cx, int cy, int size)
    {
        constexpr float kLobeDx = 0.45f;   // 圆瓣中心横向偏移
        constexpr float kLobeDy = -0.35f;  // 圆瓣中心纵向偏移（屏幕坐标向上为负）
        constexpr float kLobeR2 = 0.2025f; // 圆瓣半径 0.45 的平方

        const float scale = static_cast<float>(size);

        for (int y = -size; y <= size; y++)
        {
            const float ny = static_cast<float>(y) / scale;

            // 下方楔形：从两瓣之间一直收尖到底部
            float wedge = 0.0f;
            if (ny >= -0.15f && ny <= 0.95f)
                wedge = 0.90f * (0.95f - ny) / 1.10f;

            for (int x = -size; x <= size; x++)
            {
                const float nx = static_cast<float>(x) / scale;
                const float dl = (nx + kLobeDx) * (nx + kLobeDx) + (ny - kLobeDy) * (ny - kLobeDy);
                const float dr = (nx - kLobeDx) * (nx - kLobeDx) + (ny - kLobeDy) * (ny - kLobeDy);

                if (dl <= kLobeR2 || dr <= kLobeR2 || fabsf(nx) <= wedge)
                    display.DrawPixel(cx + x, cy + y);
            }
        }
    }

    // drawSpiralEye 画一只眩晕螺旋眼
    void drawSpiralEye(IDisplay &display, int cx, int cy, float radius, float timeSec, bool right)
    {
        constexpr int kSegments = 24;
        const float direction   = right ? -1.0f : 1.0f;

        int prevX = cx;
        int prevY = cy;
        for (int i = 1; i <= kSegments; i++)
        {
            const float u     = static_cast<float>(i) / kSegments;
            const float angle = timeSec * 5.5f * direction + u * 13.0f;
            const float rad   = radius * u;
            const int x       = cx + static_cast<int>(lroundf(rad * cosf(angle)));
            const int y       = cy + static_cast<int>(lroundf(rad * sinf(angle)));
            display.DrawLine(prevX, prevY, x, y);
            prevX = x;
            prevY = y;
        }
    }

    // drawHeartEye 画心形眼：整只眼睛变成一颗会跳动的心
    void drawHeartEye(IDisplay &display, int x, int y, float h, float timeSec, bool right)
    {
        // 两颗心错开半拍跳动，比整张脸一起缩放更俏皮
        const float phase = right ? 0.0f : 0.6f;
        const float beat  = 1.0f + 0.07f * sinf(timeSec * 2.0f * 3.14159265f * 1.05f + phase);
        const int size    = clampInt(static_cast<int>(lroundf(h * 0.98f * beat)), 3, 16);

        fillHeart(display, x, y - 1, size);
    }

    // drawRoundEye 画一只带眼皮的圆角眼
    void drawRoundEye(IDisplay &display, int x, int y, float w, float h, float pupilX, const FaceParams &params,
                      bool right)
    {
        const float angle = params.eyeTilt * kTiltMax;

        EyeGeom geom;
        geom.w      = w;
        geom.h      = h;
        geom.invW   = 1.0f / w;
        geom.invH   = 1.0f / h;
        geom.cosA   = cosf(angle);
        geom.sinA   = sinf(angle);
        geom.slant  = params.lidSlant;
        geom.squint = clampf(params.lidSquint + (right ? fmaxf(params.eyeWink, 0.0f) : fmaxf(-params.eyeWink, 0.0f)),
                             0.0f, 1.0f);
        geom.sign   = right ? -1.0f : 1.0f;

        // 旋转后的包围盒
        const int bx = static_cast<int>(w * fabsf(geom.cosA) + h * fabsf(geom.sinA)) + 2;
        const int by = static_cast<int>(w * fabsf(geom.sinA) + h * fabsf(geom.cosA)) + 2;

        for (int iy = -by; iy <= by; iy++)
        {
            for (int ix = -bx; ix <= bx; ix++)
            {
                if (insideEye(geom, static_cast<float>(ix), static_cast<float>(iy)))
                    display.DrawPixel(x + ix, y + iy);
            }
        }

        // 瞳孔：大小随情绪缩放，位置跟视线偏移；黑盘越界时会自然融进背景。
        // 眼睛压到很窄时（眨眼收尾）不画瞳孔，免得把眼白整个涂黑。
        if (h < 4.0f)
            return;

        int pupilR     = clampInt(static_cast<int>(lroundf(0.46f * fminf(w, h) * params.pupilSize)), 2, 11);
        const int maxX = clampInt(static_cast<int>(w) - pupilR - 2, 0, 64);
        const int maxY = clampInt(static_cast<int>(h) - pupilR - 1, 0, 64);
        const int px   = x + static_cast<int>(lroundf(pupilX * static_cast<float>(maxX)));
        const int py   = y + static_cast<int>(lroundf(params.pupilY * static_cast<float>(maxY)));

        display.SetDrawColor(0);
        display.DrawDisc(px, py, pupilR);
        display.SetDrawColor(1);

        // 高光只在眼白内部绘制，避免眼皮切掉后留下悬空的白点
        const int gx = px - pupilR / 2;
        const int gy = py - pupilR / 2;
        if (insideEye(geom, static_cast<float>(gx - x), static_cast<float>(gy - y)))
            display.DrawDisc(gx, gy, pupilR >= 6 ? 2 : 1);

        const int sx = px + pupilR / 2;
        const int sy = py + pupilR / 2;
        if (insideEye(geom, static_cast<float>(sx - x), static_cast<float>(sy - y)))
            display.DrawPixel(sx, sy);
    }

    void drawEye(IDisplay &display, int cx, int cy, const FaceState &state, int dx, int dy, bool right)
    {
        const FaceParams &params = state.params;

        // 左右眼轻微不对称：一只更大、略高，好奇时尤其明显
        const float asym = params.eyeAsym;
        const float wink = right ? fmaxf(params.eyeWink, 0.0f) : fmaxf(-params.eyeWink, 0.0f);
        const float w    = params.eyeW * (1.0f + (right ? -0.06f : 0.06f) * asym) * (1.0f + 0.05f * wink);
        const float h    = params.eyeH * (1.0f + (right ? -0.12f : 0.12f) * asym) * (1.0f - 0.30f * wink);
        const int x      = cx + dx;
        const int y      = cy + dy + static_cast<int>(lroundf((right ? 1.0f : -1.0f) * 2.0f * asym));

        // 眨眼闭到底：一条圆头短线
        if (h < 1.2f)
        {
            const int halfW = clampInt(static_cast<int>(lroundf(w * 0.85f)), 4, 18);
            display.DrawRBox(x - halfW, y - 1, halfW * 2 + 1, 3, 1);
            return;
        }

        if (params.eyeCross > 0.55f)
        {
            drawSpiralEye(display, x, y, fminf(w, h) * 0.95f, state.timeSec, right);
            return;
        }

        if (params.eyeHeart > 0.55f)
        {
            drawHeartEye(display, x, y, h, state.timeSec, right);
            return;
        }

        // 对眼：左眼瞳孔向右、右眼向左，困惑时两只眼睛一起“对”到鼻梁
        const float pupilX = clampf(params.pupilX + (right ? -params.pupilCross : params.pupilCross), -1.0f, 1.0f);
        drawRoundEye(display, x, y, w, h, pupilX, params, right);
    }

} // namespace

void FacePainter::Paint(IDisplay &display, const FaceState &state) const
{
    display.SetDrawColor(1);

    const int dx = static_cast<int>(lroundf(state.params.faceX));
    const int dy = static_cast<int>(lroundf(state.params.faceY));

    drawEye(display, kEyeLeftX, kEyeY, state, dx, dy, false);
    drawEye(display, kEyeRightX, kEyeY, state, dx, dy, true);
}
