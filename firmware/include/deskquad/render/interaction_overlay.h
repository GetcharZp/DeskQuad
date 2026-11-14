// interaction_overlay.h 在屏幕右上角画交互状态胶囊
//
// 胶囊只占 128×64 屏幕顶端的一小块：ASR 录音时显示跳动的电平条，
// AI 请求时显示旋转指示器，TTS 播报时显示电平条。它画在所有页面
// 之上，但不会改动页面本身的布局。

#pragma once

#include <deskquad/app/interaction_status.h>
#include <deskquad/hal/display.h>

// InteractionOverlay 绘制交互状态提示
class InteractionOverlay
{
public:
    // Paint 把状态胶囊画进当前缓冲；kind 为 None 时什么都不画
    //
    // # Params:
    //
    //	display: 显示接口
    //	status: 当前交互阶段与音频能量
    //	nowMs: 毫秒时钟，驱动动画
    void Paint(IDisplay &display, const InteractionStatus &status, uint32_t nowMs) const;
};
