// settings_store.h 把 WiFi、天气与大模型凭据保存到 NVS
//
// 使用 Preferences(NVS) 而不是 LittleFS：uploadfs 会重写整个文件系统分区，
// 存在 LittleFS 里的配置会被清掉；NVS 分区与文件系统分离，刷新固件不受影响。
// 布局不对外暴露，读写都通过 SettingsStore。

#pragma once

#include <Arduino.h>
#include <Preferences.h>

namespace deskquad
{

    // WifiSettings 是配网相关的持久化配置
    struct WifiSettings
    {
        String ssid; // 路由器 SSID
        String pass; // 路由器密码
    };

    // WeatherSettings 是天气服务相关的持久化配置
    struct WeatherSettings
    {
        String city;   // 天气城市，心知天气拼音小写
        String apiKey; // 心知天气 API 密钥
    };

    // AiSettings 是大模型相关的持久化配置
    struct AiSettings
    {
        String apiKey;  // 大模型 API 密钥
        String model;   // 模型名，空值回落到 deepseek-flash
        String baseUrl; // 推理端点，空值直连 DeepSeek
    };

    // SettingsStore 负责读写 NVS 中的配置
    class SettingsStore
    {
    public:
        // Begin 打开 NVS 命名空间，返回 false 表示 NVS 不可用
        bool Begin();

        // LoadWifi 读取 WiFi 配置
        //
        // 没有保存过配置时返回 false，out 的各字段为空。
        bool LoadWifi(WifiSettings &out);

        // SaveWifi 写入 WiFi 配置，空字段会清除对应键
        bool SaveWifi(const WifiSettings &value);

        // LoadWeather 读取天气配置
        //
        // 没有保存过密钥时返回 false，out 的各字段为空。
        bool LoadWeather(WeatherSettings &out);

        // SaveWeather 写入天气配置，空字段会清除对应键
        bool SaveWeather(const WeatherSettings &value);

        // LoadAi 读取大模型配置，没有配置过时返回 false
        bool LoadAi(AiSettings &out);

        // SaveAi 写入大模型配置，空字段会清除对应键
        bool SaveAi(const AiSettings &value);

    private:
        Preferences _prefs;
        bool _ready = false;
    };

} // namespace deskquad
