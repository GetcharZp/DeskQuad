#include <deskquad/app/interaction_controller.h>

#include <deskquad/core/serial_command_table.h>

namespace
{

    // kGestureCooldownMs 是两次手势触发之间的最小间隔
    constexpr unsigned long kGestureCooldownMs = 1500;

    // 语音命令直接对应的姿态动作，下标为 VoiceCommand 编号，None 表示由 switch 处理
    constexpr ActionId kPoseForVoice[] = {
        ActionId::None,      // None
        ActionId::Front,     // Forward
        ActionId::Back,      // Backward
        ActionId::Left,      // Left
        ActionId::Right,     // Right
        ActionId::None,      // Stop
        ActionId::Sit,       // Sit
        ActionId::Lie,       // Lie
        ActionId::None,      // Happy
        ActionId::None,      // Angry
        ActionId::None,      // Sad
        ActionId::None,      // Curious
        ActionId::None,      // Love
        ActionId::Sleep,     // Sleep
        ActionId::None,      // Free
        ActionId::HandLeft,  // HandLeft
        ActionId::HandRight, // HandRight
        ActionId::None,      // Time
        ActionId::None,      // Weather
        ActionId::None,      // Logo
        ActionId::None,      // Hello
        ActionId::None,      // Bye
    };

    constexpr uint8_t kVoiceCount = sizeof(kPoseForVoice) / sizeof(kPoseForVoice[0]);

} // namespace

InteractionController::InteractionController(AppContext &context) : _context(context) {}

// tryAgentForVoice 把社交类语音命令交给 AI
//
// # Params:
//
//	cmd: 语音命令
//	overrideText: 非空时用该文本替换融合层生成的文本
//
// # Returns:
//
//	true 表示 AI 已接手，调用方不必再做本地反应
bool InteractionController::tryAgentForVoice(VoiceCommand cmd, const char *overrideText)
{
    if (!_context.deepseek.isConfigured())
        return false;

    // 融合层先记录这次输入；队列忙时 Submit 返回空串，调用方转而执行本地反应
    const MultimodalContext ctx = _context.fusion.processVoice(static_cast<int8_t>(cmd));
    const String text           = overrideText != nullptr ? String(overrideText) : ctx.userText;
    return _context.agent.Submit(text, true).length() > 0;
}

void InteractionController::handleVoiceCommand(VoiceCommand cmd)
{
    // 扬声器反馈
    _context.speaker.play(MELODY_COMMAND_OK);
    _context.runtime.lastInteractMs = millis();
    _context.runtime.voiceTriggered = true;

    Serial.printf("[sys] voice: %d\n", static_cast<int>(cmd));

    // ----- 运动命令 (走快速通道) -----
    const uint8_t index = static_cast<uint8_t>(cmd);
    if (index < kVoiceCount && kPoseForVoice[index] != ActionId::None)
    {
        _context.motion.RequestPose(kPoseForVoice[index]);
        _context.speaker.play(MELODY_BEEP);
        return;
    }

    switch (cmd)
    {
    case VoiceCommand::Stop:
        _context.motion.ClearPendingPose(); // 停止所有动作
        _context.runtime.freeMode = false;
        _context.speaker.play(MELODY_BEEP);
        break;
    case VoiceCommand::Free:
        _context.runtime.freeMode = !_context.runtime.freeMode; // 切换自由模式
        _context.speaker.play(_context.runtime.freeMode ? MELODY_HAPPY : MELODY_DONE);
        break;

    // ----- 社交/表情命令 (AI 已启用时走 Agent + 情感融合，否则直接执行) -----
    case VoiceCommand::Hello:
        if (!tryAgentForVoice(cmd))
        {
            _context.renderer.SetEmoji(EmojiId::Happy);
            _context.speaker.play(MELODY_HELLO);
        }
        break;
    case VoiceCommand::Happy:
        if (!tryAgentForVoice(cmd))
        {
            _context.renderer.SetEmoji(EmojiId::Happy);
            _context.speaker.play(MELODY_HAPPY);
        }
        break;
    case VoiceCommand::Sad:
        if (!tryAgentForVoice(cmd))
        {
            _context.renderer.SetEmoji(EmojiId::Sad);
            _context.speaker.play(MELODY_SAD);
        }
        break;
    case VoiceCommand::Angry:
        if (!tryAgentForVoice(cmd))
            _context.renderer.SetEmoji(EmojiId::Angry);
        break;
    case VoiceCommand::Curious:
        if (!tryAgentForVoice(cmd))
            _context.renderer.SetEmoji(EmojiId::Curious);
        break;
    case VoiceCommand::Love:
        if (!tryAgentForVoice(cmd))
        {
            _context.renderer.SetEmoji(EmojiId::Love);
            _context.speaker.play(MELODY_HAPPY);
        }
        break;
    case VoiceCommand::Bye:
        if (!tryAgentForVoice(cmd))
        {
            _context.renderer.SetEmoji(EmojiId::Happy);
            _context.speaker.play(MELODY_GOODBYE);
        }
        break;

    // ----- 信息查询 (AI 优先) -----
    case VoiceCommand::Time:
        if (!tryAgentForVoice(cmd, "现在几点了"))
            _context.renderer.SetEmoji(EmojiId::Clock);
        break;
    case VoiceCommand::Weather:
        if (!tryAgentForVoice(cmd, "今天天气怎么样"))
            _context.renderer.SetEmoji(EmojiId::Weather);
        break;
    case VoiceCommand::Logo:
        _context.renderer.SetEmoji(EmojiId::Logo);
        break;

    default:
        _context.speaker.play(MELODY_ERROR);
        break;
    }
}

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

void InteractionController::PollVoice()
{
    const VoiceCommand cmd = _context.voice.Poll();
    if (cmd != VoiceCommand::None)
        handleVoiceCommand(cmd);
}

void InteractionController::PollGesture()
{
    if (!_context.camera.Poll(_frameBuffer))
        return;

    handleGesture(_context.gesture.Classify(_frameBuffer));
}
