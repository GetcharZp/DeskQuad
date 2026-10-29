// serial_command_table.h 解析串口命令行
//
// 关键字表，新增命令只需在表里加一行。
//
// 这一层不依赖 Arduino，可以被 env:native 直接编译测试。

#pragma once

#include <stdint.h>

#include <deskquad/core/states.h>

// SerialCommandKind 是串口命令的种类
enum class SerialCommandKind : uint8_t
{
    None,          // 无法识别
    Ask,           // ask <文本>，交给 AI
    AgentClear,    // ai clear，清空对话历史
    EmotionStatus, // emotion，打印当前情感状态
    EmotionReset,  // emotion reset，情感回到基线
    GestureTest,   // gesture，手动触发一次手势识别
    Pose,          // 触发一个姿态动作
    Emoji,         // 切换到指定表情页
};

// SerialCommand 是一行串口输入的解析结果
struct SerialCommand
{
    SerialCommandKind kind; // 命令种类
    ActionId pose;          // kind == Pose 时有效
    EmojiId emoji;          // kind == Emoji 时有效
    const char *argument;   // kind == Ask 时指向输入行内的文本，调用方持有该缓冲
};

// ParseSerialCommand 解析一行串口输入
//
// # Params:
//
//	line: 已去掉首尾空白的输入行
//
// # Returns:
//
//	解析结果；无法识别时 kind 为 None
SerialCommand ParseSerialCommand(const char *line);
