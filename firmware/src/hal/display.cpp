#include <deskquad/hal/display.h>

#include <deskquad/config/pins.h>

U8g2Display::U8g2Display() : _u8g2(U8G2_R0, U8X8_PIN_NONE, deskquad::kOledClockPin, deskquad::kOledDataPin) {}

void U8g2Display::Begin()
{
    _u8g2.begin();
}

void U8g2Display::SetRotation(const u8g2_cb_t *rotation)
{
    _u8g2.setDisplayRotation(rotation);
}

void U8g2Display::FirstPage()
{
    _u8g2.firstPage();
}

bool U8g2Display::NextPage()
{
    return _u8g2.nextPage();
}

void U8g2Display::Clear()
{
    _u8g2.clearDisplay();
}

void U8g2Display::SetFont(const uint8_t *font)
{
    _u8g2.setFont(font);
}

void U8g2Display::DrawXBMP(int x, int y, int w, int h, const uint8_t *bitmap)
{
    _u8g2.drawXBMP(x, y, w, h, bitmap);
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
