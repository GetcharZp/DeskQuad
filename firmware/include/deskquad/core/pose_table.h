// pose_table.h 定义姿态动作的步进序列

#pragma once

#include <stdint.h>

#include <deskquad/core/states.h>

// PoseStep 是姿态序列中的一步
struct PoseStep
{
    int16_t servo[4]; // 四个舵机的目标角度，取 kHoldAngle 表示保持不动
    uint16_t holdMs;  // 本步执行后停留的毫秒数
};

// PoseProgram 描述一个完整的姿态动作
//
// 前 loopSteps 步循环 loopCount 次，其后剩余的步只执行一次 —— 抬左手、
// 抬右手是「摆动 3 次后归位」，就靠这个结构表达。
struct PoseProgram
{
    const PoseStep *steps; // 步进序列
    uint8_t stepCount;     // 序列总步数
    uint8_t loopSteps;     // 参与循环的步数，为序列前缀
    uint8_t loopCount;     // 前缀循环次数
};

// kHoldAngle 表示该舵机本步不写入
inline constexpr int16_t kHoldAngle = -1;

// FindPoseProgram 返回编号对应的姿态序列
//
// # Returns:
//
//	序列指针；该编号不是可执行姿态时返回 nullptr
const PoseProgram *FindPoseProgram(ActionId id);
