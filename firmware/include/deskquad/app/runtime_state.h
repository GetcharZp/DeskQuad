// runtime_state.h 保存跨模块共享的运行状态
//
// 这些都是「哪一侧都能改」的少量标志，集中放一处便于追踪。

#pragma once

#include <stdint.h>

// RuntimeState 是设备当前的交互状态
struct RuntimeState
{
    bool freeMode;                // 自由模式，隔一段时间随机换个动作
    bool voiceTriggered;          // 自上次查询以来有语音输入
    bool gestureTriggered;        // 自上次查询以来有手势输入
    unsigned long lastInteractMs; // 最近一次交互的时刻

    RuntimeState() : freeMode(false), voiceTriggered(false), gestureTriggered(false), lastInteractMs(0) {}
};
