// image_data.h 声明 OLED 显示用的 XBMP 位图数据
//
// 数据本体在 src/render/image_data.cpp，全部存放在 PROGMEM 中。
// 位图由 BMP 经 PCtoLCD2002 转成 C 数组后粘贴而来，生成工具不随仓库发布。

#pragma once

#include <Arduino.h>

// 128x64 表情位图，命名与 EmojiId 一一对应
extern const uint8_t happy[] PROGMEM;
extern const uint8_t angry[] PROGMEM;
extern const uint8_t error[] PROGMEM;
extern const uint8_t curious[] PROGMEM;
extern const uint8_t love[] PROGMEM;
extern const uint8_t sad[] PROGMEM;
extern const uint8_t dizzy[] PROGMEM;

// 64x64 图标位图
extern const uint8_t rain[] PROGMEM;
extern const uint8_t sun[] PROGMEM;
extern const uint8_t cloud[] PROGMEM;
extern const uint8_t wifi[] PROGMEM;
extern const uint8_t clockIcon[] PROGMEM;
extern const uint8_t ipIcon[] PROGMEM;

// 128x22 开机 Logo
extern const uint8_t logo[] PROGMEM;
