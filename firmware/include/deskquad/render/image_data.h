// image_data.h 声明 OLED 显示用的 XBMP 位图数据
//
// 数据本体在 src/render/image_data.cpp，全部存放在 PROGMEM 中。
// 位图由 BMP 经 PCtoLCD2002 转成 C 数组后粘贴而来，生成工具不随仓库发布。
//
// 表情页已改为 FacePainter 程序化绘制，这里只保留图标与开机 Logo。

#pragma once

#include <Arduino.h>

// 64x64 图标位图
extern const uint8_t rain[] PROGMEM;
extern const uint8_t sun[] PROGMEM;
extern const uint8_t cloud[] PROGMEM;
extern const uint8_t wifi[] PROGMEM;
extern const uint8_t clockIcon[] PROGMEM;
extern const uint8_t ipIcon[] PROGMEM;

// 128x64 开机 Logo
extern const uint8_t logo[] PROGMEM;
