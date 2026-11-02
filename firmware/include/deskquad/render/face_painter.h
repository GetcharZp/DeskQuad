// face_painter.h 把 FaceState 画到 OLED 上
//
// 只做“参数 → 图元”的映射，不保存状态，因此同一帧可以被反复重绘而
// 没有副作用。坐标以 128×64 屏幕为准。
#pragma once

#include <deskquad/hal/display.h>
#include <deskquad/render/face_model.h>

// FacePainter 绘制程序化表情与特效
class FacePainter
{
public:
    // Paint 绘制一整帧表情
    //
    // # Params:
    //
    //	display: 显示接口，调用前缓冲应已清空
    //	state: 当前帧的表情快照
    void Paint(IDisplay &display, const FaceState &state) const;
};
