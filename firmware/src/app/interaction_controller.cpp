#include <deskquad/app/interaction_controller.h>

#include <deskquad/core/serial_command_table.h>

namespace
{
    // kGestureCooldownMs 是两次手势触发之间的最小间隔
    constexpr unsigned long kGestureCooldownMs = 1500;
} // namespace

InteractionController::InteractionController(AppContext &context) : _context(context) {}

void InteractionController::handleGesture(const GestureResult &result)
{
    // 手离开画面后解除锁定，下一次同类手势才能再次触发
    if (result.gesture == GESTURE_NONE)
    {
        _gestureLatched = false;
        return;
    }

    const unsigned long now = millis();
    if (_gestureLatched && result.gesture == _lastGestureClass)
        return;
    if (_lastGestureClass != GESTURE_NONE && now - _lastGestureMs < kGestureCooldownMs)
        return;

    _gestureLatched   = true;
    _lastGestureClass = result.gesture;
    _lastGestureMs    = now;

    Serial.printf("[sys] gesture: class=%d conf=%.2f area=%d box=%dx%d tips=%d reverse=%d\n", result.gesture,
                  result.confidence, result.blobArea, result.bboxWidth, result.bboxHeight, result.fingerCount,
                  result.waveReversals);

    _context.speaker.play(MELODY_BEEP);
    _context.runtime.lastInteractMs   = now;
    _context.runtime.gestureTriggered = true;

    // 1. 通过多模态融合层更新情感
    const MultimodalContext ctx = _context.fusion.processGesture((int8_t)result.gesture, result.confidence);

    // 2. AI 已启用且队列空闲时交给 Agent 处理
    if (_context.deepseek.isConfigured() && _context.agent.Submit(ctx.userText, true).length() > 0)
        return;

    // 3. AI 未启用或正忙时，手势直接映射为动作
    switch (result.gesture)
    {
    case GESTURE_WAVE:
        _context.runtime.freeMode = !_context.runtime.freeMode;
        _context.speaker.play(_context.runtime.freeMode ? MELODY_HAPPY : MELODY_DONE);
        break;
    case GESTURE_PALM:
        _context.motion.RequestPose(ActionId::Lie); // 趴下 (停)
        break;
    case GESTURE_FIST:
        _context.motion.TriggerEmotion(ACTION_NOD); // 点头 (同意)
        break;
    case GESTURE_POINT:
        _context.motion.TriggerEmotion(ACTION_BOW); // 鞠躬 (礼貌)
        break;
    default:
        break;
    }
}

void InteractionController::PollSerial()
{
    if (Serial.available() <= 0)
        return;

    String received = Serial.readStringUntil('\n');
    received.trim();
    Serial.printf("[sys] console: %s\n", received.c_str());

    const SerialCommand command = ParseSerialCommand(received.c_str());
    switch (command.kind)
    {
    case SerialCommandKind::Ask:
        if (command.argument[0] != '\0')
        {
            Serial.printf("[sys] agent: Question via serial: \"%s\"\n", command.argument);
            _context.agent.Submit(command.argument);
        }
        break;
    case SerialCommandKind::AgentClear:
        _context.deepseek.clearHistory();
        Serial.println("[sys] agent: history cleared");
        break;
    case SerialCommandKind::EmotionStatus: {
        const PADState &pad = _context.emotion.GetState();
        Serial.printf("[sys] emotion: %s (P=%.2f A=%.2f D=%.2f)\n", _context.emotion.GetLabel(), pad.pleasure,
                      pad.arousal, pad.dominance);
        break;
    }
    case SerialCommandKind::EmotionReset:
        _context.emotion.Reset();
        break;
    case SerialCommandKind::GestureTest:
        if (_context.camera.Capture(_frameBuffer))
        {
            const GestureResult result = _context.gesture.Classify(_frameBuffer);
            Serial.printf("[sys] gesture: class=%d conf=%.2f area=%d box=%dx%d tips=%d reverse=%d bg=%d\n",
                          result.gesture, result.confidence, result.blobArea, result.bboxWidth, result.bboxHeight,
                          result.fingerCount, result.waveReversals, _context.gesture.IsBackgroundReady() ? 1 : 0);
        }
        break;
    case SerialCommandKind::Pose:
        _context.motion.RequestPose(command.pose);
        break;
    case SerialCommandKind::Emoji:
        _context.renderer.SetEmoji(command.emoji);
        break;
    case SerialCommandKind::None:
        break;
    }
}

void InteractionController::PollGesture()
{
    if (!_context.camera.Poll(_frameBuffer))
        return;

    handleGesture(_context.gesture.Classify(_frameBuffer));
}
