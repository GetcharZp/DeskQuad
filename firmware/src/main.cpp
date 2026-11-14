// main.cpp 装配各子系统并驱动主循环
//
// 这里只做编排：具体的硬件操作在 hal/，动作在 motion/，页面在 render/，
// 路由在 web/，AI 在 app/agent_service。

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <WiFi.h>

#include <deskquad/agent/deepseek_agent.h>
#include <deskquad/app/agent_service.h>
#include <deskquad/app/app_context.h>
#include <deskquad/app/interaction_controller.h>
#include <deskquad/app/interaction_status.h>
#include <deskquad/app/runtime_state.h>
#include <deskquad/app/weather_service.h>
#include <deskquad/config/app_config.h>
#include <deskquad/config/pins.h>
#include <deskquad/core/states.h>
#include <deskquad/emotion/emotion_engine.h>
#include <deskquad/emotion/multimodal_fusion.h>
#include <deskquad/hal/battery.h>
#include <deskquad/hal/display.h>
#include <deskquad/hal/servo_bus.h>
#include <deskquad/hal/settings_store.h>
#include <deskquad/hal/speaker.h>
#include <deskquad/hal/time_service.h>
#include <deskquad/motion/motion_director.h>
#include <deskquad/perception/gesture_recognizer.h>
#include <deskquad/perception/ov2640.h>
#include <deskquad/render/expression_renderer.h>
#include <deskquad/render/image_data.h>
#include <deskquad/render/views.h>
#include <deskquad/voice/voice_service.h>
#include <deskquad/web/web_router.h>

namespace
{
    // 开机配网提示页的停留时间
    constexpr unsigned long kSplashMs = 3000;

    // 自由模式下两次随机动作的最小间隔
    constexpr unsigned long kFreeModeIntervalMs = 3000;
    unsigned long gLastFreePoseMs               = 0;

    // SNTP 只在 WiFi 从断开变为连接时启动一次；断线重连后重新同步
    bool gNtpStarted = false;

    // 自由模式随机挑选的范围：姿态 Front..RandomEmote，表情 Happy..Dizzy
    constexpr uint8_t kPoseFirst  = static_cast<uint8_t>(ActionId::Front);
    constexpr uint8_t kPoseLast   = static_cast<uint8_t>(ActionId::RandomEmote);
    constexpr uint8_t kEmojiFirst = static_cast<uint8_t>(EmojiId::Happy);
    constexpr uint8_t kEmojiLast  = static_cast<uint8_t>(EmojiId::Dizzy);
} // namespace

const deskquad::Config gConfig = deskquad::DefaultConfig();

// 硬件与外设
BatteryMonitor batteryMonitor(gConfig);
ServoBus servoBus;
U8g2Display display;
Camera cameraModule;
Speaker speaker;
VoiceService voiceService;

// 显示与时间
WeatherView weatherView;
ClockView clockView;
ExpressionRenderer renderer(display, weatherView, clockView);
WeatherService weatherService(gConfig);
TimeService timeService(gConfig);

// 运动与情感
MotionDirector motionDirector(servoBus);
EmotionEngine emotionEngine;
GestureRecognizer gestureRecognizer;
MultimodalFusion fusion;

// 网络与 AI
AsyncWebServer server(gConfig.httpPort);
DeepSeekAgent aiAgent;
AgentService agentService;
deskquad::SettingsStore settingsStore;

RuntimeState runtime;

AppContext appContext{
    gConfig,       settingsStore, motionDirector,    renderer,    speaker,      batteryMonitor, agentService, aiAgent,
    emotionEngine, fusion,        gestureRecognizer, voiceService, cameraModule, weatherService, runtime,
};

// 串口、手势两条本地输入通道的处理逻辑
InteractionController interaction(appContext);

void loadWeatherConfig()
{
    deskquad::WeatherSettings settings;
    if (!settingsStore.LoadWeather(settings))
        return;

    if (settings.city.length() > 0)
        weatherService.SetCity(settings.city);
    weatherService.SetApiKey(settings.apiKey);
}

void loadWiFiConfig()
{
    deskquad::WifiSettings settings;
    if (!settingsStore.LoadWifi(settings))
    {
        Serial.println("[sys] wifi: no saved config, staying on hotspot");
        return;
    }

    Serial.printf("[sys] wifi: connecting to saved ssid %s\n", settings.ssid.c_str());
    WiFi.begin(settings.ssid.c_str(), settings.pass.c_str());
    const unsigned long startAttemptTime = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 5000)
    {
        delay(500);
    }

    if (WiFi.status() != WL_CONNECTED)
    {
        Serial.println("WiFi connection failed, keeping captive portal...");
        return;
    }

    Serial.println("WiFi connected");
}

void setup()
{
    Serial.begin(115200);
    // 串口命令按行读取，限制等待时间，避免半行输入阻塞主循环
    Serial.setTimeout(50);

    // WiFi 驱动默认会把凭据和射频校准数据写进默认 nvs 分区。该分区只有 20KB，
    // 写满后 Arduino 核会在启动时整片擦除它。用户配置已经放到独立的 nvs_cfg
    // 分区（见 partitions.csv / SettingsStore），这里再关掉驱动侧持久化，
    // 减少默认分区的擦写，避免它反复写满后拖慢启动。
    WiFi.persistent(false);

    display.Begin();
    display.SetRotation(U8G2_R2);
    // 开机 Logo（128×64 全屏位图）
    display.FirstPage();
    do
    {
        display.DrawXBMP(0, 0, 128, 64, logo);
    } while (display.NextPage());

    batteryMonitor.Begin();
    LittleFS.begin(true);   // 挂载失败时自动格式化
    settingsStore.Begin();  // 打开 NVS 配置存储
    servoBus.Begin();       // 挂载四路舵机并回到中立位
    motionDirector.Begin(); // 关联情感动作的舵机写入接口

    //---------------初始化新模块--------------------------
    // 扬声器只复位播放状态；音频输出在 VoiceService 初始化 I2S 后注入
    speaker.begin();

    // 初始化摄像头模块 (OV2640 + 灰度捕获)
    // 用于 GestureRecognizer 手势识别；挥手依赖时序，间隔不能太长
    if (cameraModule.Begin())
    {
        Serial.println("[sys] camera: ready");
        cameraModule.SetIntervalMs(200); // 每 200ms 捕获一帧
    }
    else
    {
        Serial.println("[warn] camera: not found, skipped");
    }

    //---------------初始化情感计算引擎--------------------------
    emotionEngine.SetDebug(false);
    // 人格: 外倾、宜人、尽责、低神经质、开放
    emotionEngine.SetPersonality({0.75f, 0.80f, 0.60f, 0.30f, 0.70f});
    emotionEngine.SetDecay(0.97f, 0.98f, 0.99f); // 缓慢回归中性
    Serial.println("[sys] emotion: PAD engine initialized");

    // 连接多模态融合层 → 情感引擎
    fusion.attachEmotionEngine(&emotionEngine);
    fusion.setDebug(false);

    // 手势识别引擎 (背景差分 + 轮廓规则，启动时先保持画面无手)
    Serial.println("[sys] gesture: rule-based recognizer ready");

    //---------------初始化 AI Agent--------------------------
    // 从 NVS 加载 AI 配置 (apiKey, model, baseUrl)
    deskquad::AiSettings aiSettings;
    if (settingsStore.LoadAi(aiSettings))
    {
        aiAgent.configure(aiSettings.apiKey, aiSettings.model, aiSettings.baseUrl);
        Serial.println("[sys] agent: configured from NVS");
    }
    else
    {
        Serial.println("[sys] agent: no credentials, AI disabled");
    }

    // 关联各子系统；不接线时 AgentService 会静默丢弃所有 AI 请求
    agentService.Begin(aiAgent, fusion, emotionEngine, motionDirector, speaker, renderer);

    // 豆包 API Key 由网页写入 NVS，缺省时语音任务仍启动，等用户补填后再用
    deskquad::VoiceSettings voiceSettings;
    if (settingsStore.LoadVoice(voiceSettings))
    {
        voiceService.SetApiKey(voiceSettings.apiKey.c_str());
        Serial.println("[sys] voice: api key loaded from NVS");
    }

    // 语音链路依赖 WiFi STA 和 PSRAM；任务启动后先空闲等待唤醒
    // Begin 成功后会注入功放输出，开机音效在配网提示页期间播放
    voiceService.Begin(agentService, runtime, speaker);

    // 设置WiFi为热点模式
    WiFi.softAP(gConfig.apSsid, gConfig.apPassword);
    Serial.println("[sys] 热点已启动");
    // 热点默认 IP：192.168.4.1
    Serial.print("[sys] 访问地址: ");
    Serial.print(WiFi.softAPIP());
    loadWiFiConfig();
    loadWeatherConfig();
    // 路由只注册一次；无论 STA 是否连上，Web 控制台都要可用
    RegisterAllRoutes(server, appContext);

    // 配网提示页：停留几秒让用户看到热点地址，然后进入正常界面
    display.Clear();
    display.FirstPage();
    do
    {
        display.SetFont(u8g2_font_ncenB08_tr);
        display.DrawXBMP(0, 0, 64, 64, ipIcon);
        display.DrawStr(64, 10, "WIFI_AP");
        display.DrawStr(64, 27, "DeskQuad");
        display.DrawStr(64, 43, "192.168.4.1");
        display.DrawStr(64, 60, "WIFI CTRL");
    } while (display.NextPage());

    // 开机音效是非阻塞的，用配网提示页的停留时间驱动 Speaker
    speaker.play(MELODY_STARTUP);
    const unsigned long splashStart = millis();
    while (millis() - splashStart < kSplashMs)
    {
        speaker.loop();
        delay(10);
    }
    renderer.Begin();
}

void loop()
{
    //---------------输入通道轮询--------------------------
    interaction.PollSerial();
    interaction.PollGesture();

    //---------------情感状态衰减 (每 1s) -----------------------
    emotionEngine.Update();

    //---------------扬声器状态更新--------------------------
    speaker.loop();

    //---------------情感动作更新 (非阻塞)-------------------
    motionDirector.Update();

    // 电量采样是非阻塞的
    batteryMonitor.Update();
    if (runtime.freeMode && !voiceService.IsBusy() && millis() - gLastFreePoseMs >= kFreeModeIntervalMs)
    {
        gLastFreePoseMs = millis();
        motionDirector.RequestPose(static_cast<ActionId>(random(kPoseFirst, kPoseLast + 1)));
    }
    // 情感动作优先；进行中时姿态保持挂起，下个循环再试
    if (motionDirector.PendingPose() == ActionId::RandomEmote && !motionDirector.IsPlayingEmotion())
    {
        renderer.SetEmoji(static_cast<EmojiId>(random(kEmojiFirst, kEmojiLast + 1)));
        motionDirector.ClearPendingPose();
    }
    else
    {
        motionDirector.TickPoses();
    }

    // 姿态播放完毕后再播放提示音，与阻塞实现下的时序一致
    Melody doneMelody;
    if (motionDirector.ConsumePoseFinished(doneMelody))
    {
        speaker.play(doneMelody);
    }

    // WiFi 可能在开机后才由网页配置，连接状态变化时补启 SNTP，
    // 否则默认时间页会一直停在 SYNC。
    const bool wifiConnected = WiFi.status() == WL_CONNECTED;
    if (wifiConnected && !gNtpStarted)
    {
        timeService.Begin();
        gNtpStarted = true;
    }
    else if (!wifiConnected)
    {
        gNtpStarted = false;
    }

    // 与原实现一致：只有停留在天气页时才发起查询
    if (renderer.Current() == EmojiId::Weather)
    {
        weatherService.Update(wifiConnected);
    }

    // 表情动画的输入：PAD 情感连续调制，扬声器音符驱动眼睛弹跳，
    // 刚交互过的 1.5s 内看向用户并放大瞳孔。
    const PADState &pad = emotionEngine.GetState();
    const unsigned long nowMs = millis();
    FaceInputs faceInputs;
    faceInputs.pleasure    = pad.pleasure;
    faceInputs.arousal     = pad.arousal;
    faceInputs.dominance   = pad.dominance;
    faceInputs.noteHz      = voiceService.IsSpeaking() ? voiceService.LevelHz() : speaker.CurrentNoteFreq();
    faceInputs.interacting = runtime.lastInteractMs != 0 && nowMs - runtime.lastInteractMs < 1500;

    // ASR/TTS 由语音任务直接给出；没有语音活动时再检查 AI 请求，
    // Web/手势/串口触发的对话同样会显示“思考中”。
    InteractionStatus interactionStatus = {};
    interactionStatus.kind              = voiceService.Activity();
    interactionStatus.level             = voiceService.Level();
    if (interactionStatus.kind == InteractionKind::None &&
        agentService.Activity() == InteractionKind::Thinking)
        interactionStatus.kind = InteractionKind::Thinking;

    renderer.Render(weatherService.Data(), timeService, wifiConnected, faceInputs, interactionStatus);

    // AI 请求是阻塞的 HTTP 调用。Pump 之前先强制刷一帧，确保“思考中”胶囊
    // 已经落在屏幕上；请求期间画面不再刷新，但提示不会丢。
    if (agentService.Activity() == InteractionKind::Thinking)
    {
        if (interactionStatus.kind == InteractionKind::None)
            interactionStatus.kind = InteractionKind::Thinking;
        renderer.Render(weatherService.Data(), timeService, wifiConnected, faceInputs, interactionStatus, true);
    }
    agentService.Pump();
}
