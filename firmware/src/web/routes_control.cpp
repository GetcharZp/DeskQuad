#include <deskquad/web/routes_control.h>

#include <Arduino.h>

#include <deskquad/web/response.h>

namespace
{

    // 姿态路由：路径与动作编号一一对应
    struct PoseRoute
    {
        const char *path;
        ActionId pose;
    };

    const PoseRoute kPoseRoutes[] = {
        {"/api/front", ActionId::Front}, {"/api/back", ActionId::Back},         {"/api/left", ActionId::Left},
        {"/api/right", ActionId::Right}, {"/api/handleft", ActionId::HandLeft}, {"/api/handright", ActionId::HandRight},
        {"/api/sit", ActionId::Sit},     {"/api/lie", ActionId::Lie},           {"/api/sleep", ActionId::Sleep},
    };

    // 表情路由：路径与表情编号一一对应
    struct EmojiRoute
    {
        const char *path;
        EmojiId emoji;
    };

    const EmojiRoute kEmojiRoutes[] = {
        {"/api/happy", EmojiId::Happy},     {"/api/angry", EmojiId::Angry}, {"/api/curious", EmojiId::Curious},
        {"/api/logo", EmojiId::Logo},       {"/api/error", EmojiId::Error}, {"/api/love", EmojiId::Love},
        {"/api/sad", EmojiId::Sad},         {"/api/dizzy", EmojiId::Dizzy}, {"/api/clock", EmojiId::Clock},
        {"/api/weather", EmojiId::Weather},
    };

    // 外设状态路由：值由 context 现取，返回 "1" / "0"
    struct StatusRoute
    {
        const char *path;
        bool (*value)(AppContext &context);
    };

    const StatusRoute kStatusRoutes[] = {
        {"/api/voiceStatus", [](AppContext &context) { return context.voice.IsAvailable(); }},
        {"/api/cameraStatus", [](AppContext &context) { return context.camera.IsAvailable(); }},
        {"/api/speakerStatus", [](AppContext &context) { return context.speaker.isPlaying(); }},
    };

} // namespace

void RegisterControlRoutes(AsyncWebServer &server, AppContext &context)
{
    for (const PoseRoute &route : kPoseRoutes)
    {
        server.on(route.path, HTTP_GET, [&context, pose = route.pose](AsyncWebServerRequest *request) {
            context.motion.RequestPose(pose);
            deskquad::SendOk(request);
        });
    }

    for (const EmojiRoute &route : kEmojiRoutes)
    {
        server.on(route.path, HTTP_GET, [&context, emoji = route.emoji](AsyncWebServerRequest *request) {
            context.renderer.SetEmoji(emoji);
            deskquad::SendOk(request);
        });
    }

    // 自由模式
    server.on("/api/free", HTTP_GET, [&context](AsyncWebServerRequest *request) {
        context.runtime.freeMode = true;
        deskquad::SendOk(request);
    });
    server.on("/api/offfree", HTTP_GET, [&context](AsyncWebServerRequest *request) {
        context.runtime.freeMode = false;
        deskquad::SendOk(request);
    });

    // 电池
    server.on("/api/batteryVoltage", HTTP_GET, [&context](AsyncWebServerRequest *request) {
        request->send(200, "text/plain", String(context.battery.Voltage()));
    });
    server.on("/api/batteryPercentage", HTTP_GET, [&context](AsyncWebServerRequest *request) {
        request->send(200, "text/plain", String(context.battery.Percentage()));
    });

    // 外设状态
    for (const StatusRoute &route : kStatusRoutes)
    {
        server.on(route.path, HTTP_GET, [&context, value = route.value](AsyncWebServerRequest *request) {
            request->send(200, "text/plain", value(context) ? "1" : "0");
        });
    }
    server.on("/api/speakerMute", HTTP_GET, [&context](AsyncWebServerRequest *request) {
        context.speaker.setMute(!context.speaker.isMuted());
        request->send(200, "text/plain", context.speaker.isMuted() ? "1" : "0");
    });

    // 交互查询：读取后清空标志，页面据此做一次性提示
    server.on("/api/lastInteraction", HTTP_GET, [&context](AsyncWebServerRequest *request) {
        RuntimeState &runtime    = context.runtime;
        const String info        = "Voice:" + String(runtime.voiceTriggered ? 1 : 0) +
                                   ",Gesture:" + String(runtime.gestureTriggered ? 1 : 0) +
                                   ",Since:" + String((millis() - runtime.lastInteractMs) / 1000);
        runtime.voiceTriggered   = false;
        runtime.gestureTriggered = false;
        request->send(200, "text/plain", info);
    });
}
