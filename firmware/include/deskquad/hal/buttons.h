// buttons.h 读取两个物理按键
//
// 中断服务程序只置标志，消抖在中断里完成；主循环通过 Consume* 取走事件。

#pragma once

#include <stdint.h>

// Buttons 管理两个按键
class Buttons
{
public:
    // Begin 配置引脚并挂载中断
    //
    // 按键 1 接地触发（内部上拉，下降沿），按键 2 接 3.3V 触发（上升沿）。
    void Begin();

    // ConsumeFirst 取走一次按键 1 事件
    //
    // # Returns:
    //
    //	true 表示自上次调用以来按键 1 被按下过
    bool ConsumeFirst();

    // ConsumeSecond 取走一次按键 2 事件
    bool ConsumeSecond();
};
