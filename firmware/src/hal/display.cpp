#include <deskquad/hal/display.h>

#include <string.h>

#include <deskquad/config/pins.h>

namespace
{
    // 屏幕缓冲大小：128×64 单色，每字节 8 个纵向像素
    constexpr size_t kBufferBytes = 128 * 64 / 8;
} // namespace

U8g2Display::U8g2Display()
    : _u8g2(U8G2_R0, U8X8_PIN_NONE, deskquad::kOledClockPin, deskquad::kOledDataPin), _sentValid(false)
{
    memset(_sent, 0, sizeof(_sent));
}

void U8g2Display::Begin()
{
    _u8g2.begin();
    _sentValid = false;
}

void U8g2Display::SetRotation(const u8g2_cb_t *rotation)
{
    _u8g2.setDisplayRotation(rotation);
}

void U8g2Display::FirstPage()
{
    // 全缓冲模式下 firstPage 会清空缓冲并把 tile 行归零，
    // sendBuffer 依赖 tile 行信息，所以这里必须保留这次调用。
    _u8g2.firstPage();
}

bool U8g2Display::NextPage()
{
    SendBuffer();
    return false;
}

void U8g2Display::ClearBuffer()
{
    _u8g2.clearBuffer();
}

// SendBuffer 只把发生变化的 8×8 图块矩形发给屏幕
//
// 整屏 1KB 走 400kHz I2C 约 23ms，按 30fps 刷新会吃掉大半主循环；
// 表情动画通常只改动眼睛/嘴附近的图块，因此先做图块级差分，再用
// U8g2 的 updateDisplayArea 只发变化区域。
void U8g2Display::SendBuffer()
{
    const uint8_t *current = _u8g2.getBufferPtr();
    if (!_sentValid)
    {
        _u8g2.sendBuffer();
        memcpy(_sent, current, kBufferBytes);
        _sentValid = true;
        return;
    }

    constexpr int kTilesX  = 128 / 8;
    constexpr int kTilesY  = 64 / 8;
    constexpr int kTileRow = 128; // 每个 8px 高的 tile 行占 128 字节

    int minTx = kTilesX;
    int maxTx = -1;
    int minTy = kTilesY;
    int maxTy = -1;

    for (int ty = 0; ty < kTilesY; ty++)
    {
        const uint8_t *currentRow = current + ty * kTileRow;
        const uint8_t *sentRow    = _sent + ty * kTileRow;
        for (int tx = 0; tx < kTilesX; tx++)
        {
            if (memcmp(currentRow + tx * 8, sentRow + tx * 8, 8) == 0)
                continue;

            if (tx < minTx)
                minTx = tx;
            if (tx > maxTx)
                maxTx = tx;
            if (ty < minTy)
                minTy = ty;
            if (ty > maxTy)
                maxTy = ty;
        }
    }

    if (maxTx < 0)
        return; // 画面没有变化

    _u8g2.updateDisplayArea(static_cast<uint8_t>(minTx), static_cast<uint8_t>(minTy),
                            static_cast<uint8_t>(maxTx - minTx + 1), static_cast<uint8_t>(maxTy - minTy + 1));
    memcpy(_sent, current, kBufferBytes);
}

void U8g2Display::Clear()
{
    _u8g2.clearBuffer();
}

void U8g2Display::SetFont(const uint8_t *font)
{
    _u8g2.setFont(font);
}

void U8g2Display::SetDrawColor(uint8_t color)
{
    _u8g2.setDrawColor(color);
}

void U8g2Display::DrawStr(int x, int y, const char *text)
{
    _u8g2.drawStr(x, y, text);
}

void U8g2Display::SetCursor(int x, int y)
{
    _u8g2.setCursor(x, y);
}

void U8g2Display::Print(const char *text)
{
    _u8g2.print(text);
}

void U8g2Display::DrawXBMP(int x, int y, int w, int h, const uint8_t *bitmap)
{
    _u8g2.drawXBMP(x, y, w, h, bitmap);
}

void U8g2Display::DrawPixel(int x, int y)
{
    _u8g2.drawPixel(x, y);
}

void U8g2Display::DrawLine(int x0, int y0, int x1, int y1)
{
    _u8g2.drawLine(x0, y0, x1, y1);
}

void U8g2Display::DrawBox(int x, int y, int w, int h)
{
    _u8g2.drawBox(x, y, w, h);
}

void U8g2Display::DrawRBox(int x, int y, int w, int h, int r)
{
    _u8g2.drawRBox(x, y, w, h, r);
}

void U8g2Display::DrawFrame(int x, int y, int w, int h)
{
    _u8g2.drawFrame(x, y, w, h);
}

void U8g2Display::DrawCircle(int x, int y, int r)
{
    _u8g2.drawCircle(x, y, r, U8G2_DRAW_ALL);
}

void U8g2Display::DrawDisc(int x, int y, int r)
{
    _u8g2.drawDisc(x, y, r, U8G2_DRAW_ALL);
}

void U8g2Display::DrawFilledEllipse(int x, int y, int rx, int ry)
{
    _u8g2.drawFilledEllipse(x, y, rx, ry, U8G2_DRAW_ALL);
}

void U8g2Display::DrawArc(int x, int y, int r, uint8_t start, uint8_t end)
{
    _u8g2.drawArc(x, y, r, start, end);
}
