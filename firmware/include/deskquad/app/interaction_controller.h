// interaction_controller.h 集中处理串口与手势两条本地输入通道
//
// 语音输入由 VoiceService 独立任务处理，不再经过这里。每条通道最终
// 都会落到运动、表情、音效或 AI 对话上。

#pragma once

#include <Arduino.h>
#include <stdint.h>

#include <deskquad/app/app_context.h>
#include <deskquad/core/states.h>
#include <deskquad/perception/gesture_recognizer.h>

// InteractionController 把设备侧输入转换成具体反应
class InteractionController
{
public:
    // 构造函数
    //
    // # Params:
    //
    //	context: 各子系统引用，生命周期需长于本对象
    explicit InteractionController(AppContext &context);

    // PollSerial 读取并执行一行串口命令，无输入时立即返回
    void PollSerial();

    // PollGesture 按摄像头间隔抓帧并做手势识别
    void PollGesture();

private:
    void handleGesture(const GestureResult &result);

    AppContext &_context;
    uint8_t _frameBuffer[deskquad::kGesturePixelCount]; // 80×60 灰度缓冲，GestureRecognizer 的输入

    // 同一手势在手移开前只触发一次
    bool _gestureLatched           = false;
    GestureClass _lastGestureClass = GESTURE_NONE;
    unsigned long _lastGestureMs   = 0;
};
