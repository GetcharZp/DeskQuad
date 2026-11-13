// voice_config.h 定义豆包实时语音的参数
//
// API Key 不写在固件里，由网页写入 NVS 后交给 VoiceService；
// 产品化时应改为服务端下发短期 token。

#pragma once

#include <stddef.h>
#include <stdint.h>

namespace deskquad::voice
{

    // API Key 的缓冲区长度，覆盖常见的 UUID 与长 token
    inline constexpr size_t kApiKeyMax = 128;

    inline constexpr char kHost[]     = "openspeech.bytedance.com";
    inline constexpr uint16_t kPort   = 443;
    inline constexpr char kAsrPath[]  = "/api/v3/sauc/bigmodel_async";
    inline constexpr char kTtsPath[]  = "/api/v3/tts/bidirection";

    inline constexpr char kAsrResourceId[] = "volc.seedasr.sauc.duration";
    inline constexpr char kTtsResourceId[] = "seed-tts-2.0";
    inline constexpr char kSpeaker[]       = "zh_female_gaolengyujie_uranus_bigtts";

    // 麦克风与 TTS 统一使用 16kHz / 16bit / 单声道
    inline constexpr uint32_t kSampleRate = 16000;

    // 一句话最长 12 秒；静音 640ms（32 个 20ms 帧）后判定说完
    inline constexpr uint32_t kMaxUtteranceMs = 12000;
    inline constexpr uint32_t kSilenceFrames  = 32;
    inline constexpr uint32_t kStartFrames    = 3;

    // 触发后保留的预录时长，避免切掉第一个字
    inline constexpr uint32_t kPreRollSamples = 8000; // 500ms

    // 一次唤醒后的连续对话窗口；15s 内没有新对话就结束会话，
    // 后续说话需要重新唤醒，避免长时间占用语音会话
    inline constexpr uint32_t kConversationWindowMs = 15000;

    // 网络阶段超时
    inline constexpr uint32_t kAsrFinalTimeoutMs = 6000;
    inline constexpr uint32_t kAgentTimeoutMs    = 20000;
    inline constexpr uint32_t kTtsTimeoutMs      = 20000;

    // 能量 VAD 的下限；实际阈值会跟随环境噪声自适应
    inline constexpr uint32_t kStartRmsFloor = 900;
    inline constexpr uint32_t kEndRmsFloor   = 500;

    // 唤醒词在 ASR 文本里做子串匹配，大小写不敏感
    inline constexpr const char *kWakeWords[] = {"小呆", "呆呆", "deskquad", "xiaodai"};

} // namespace deskquad::voice
