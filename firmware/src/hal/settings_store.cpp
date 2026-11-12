#include <deskquad/hal/settings_store.h>

#include <nvs_flash.h>

namespace
{

    constexpr const char *kNamespace = "deskquad";

    // 配置写在独立分区 nvs_cfg（见 partitions.csv）。默认 nvs 分区由 WiFi 驱动和
    // PHY 射频校准数据反复写入，20KB 写满后 Arduino 核会在启动时整片擦除，
    // 用户配置放在这里就不会随它一起丢失。
    constexpr const char *kPartition       = "nvs_cfg";
    constexpr const char *kLegacyPartition = "nvs";

    // 旧固件把配置存在默认 nvs 分区；用 schema 标记只迁移一次
    constexpr const char *kSchemaKey = "schema";
    constexpr uint8_t kSchemaVersion = 1;

    // NVS 键名最长 15 个字符
    constexpr const char *kWifiSsid      = "wifi_ssid";
    constexpr const char *kWifiPass      = "wifi_pass";
    constexpr const char *kWeatherCity   = "weather_city";
    constexpr const char *kWeatherApiKey = "weather_key";
    constexpr const char *kAiKey         = "ai_key";
    constexpr const char *kAiModel       = "ai_model";
    constexpr const char *kAiBaseUrl     = "ai_base_url";
    constexpr const char *kVoiceApiKey   = "voice_key";

    constexpr const char *kAllKeys[] = {
        kWifiSsid,
        kWifiPass,
        kWeatherCity,
        kWeatherApiKey,
        kAiKey,
        kAiModel,
        kAiBaseUrl,
        kVoiceApiKey,
    };

    // MigrateLegacy 把旧版默认 nvs 分区里的配置搬到 nvs_cfg，升级后不用重新填写。
    // 读取失败或没有旧数据时也写入 schema，避免每次启动都重复尝试。
    void MigrateLegacy(Preferences &prefs)
    {
        if (prefs.getUChar(kSchemaKey, 0) >= kSchemaVersion)
            return;

        uint8_t copied = 0;
        Preferences legacy;
        if (legacy.begin(kNamespace, true, kLegacyPartition))
        {
            for (const char *key : kAllKeys)
            {
                // 新分区已经有值的键优先保留，避免覆盖用户更新过的配置
                if (prefs.isKey(key) || !legacy.isKey(key))
                    continue;

                const String value = legacy.getString(key, "");
                if (value.length() > 0 && prefs.putString(key, value) == value.length())
                    ++copied;
            }
            legacy.end();
        }

        prefs.putUChar(kSchemaKey, kSchemaVersion);
        if (copied > 0)
            Serial.printf("[sys] settings: migrated %u keys from legacy NVS\n", static_cast<unsigned>(copied));
    }

    // SaveString 写入字符串并回读校验；空值表示清除该键
    bool SaveString(Preferences &prefs, const char *key, const String &value)
    {
        if (value.length() == 0)
            return !prefs.isKey(key) || prefs.remove(key);
        if (prefs.putString(key, value) != value.length())
            return false;

        // 回读一次，确认配置真的落盘；写失败时让接口返回错误，而不是提示「已保存」
        return prefs.getString(key, "") == value;
    }

} // namespace

bool deskquad::SettingsStore::Begin()
{
    if (_ready)
        return true;

    _ready = _prefs.begin(kNamespace, false, kPartition);
    if (!_ready)
    {
        // 分区第一次使用时内容可能还不是合法的 NVS 布局（例如复用了旧 app1 的
        // 尾部扇区）。里面没有可用配置，擦一次再打开；失败则说明 NVS 本身不可用。
        nvs_flash_erase_partition(kPartition);
        _ready = _prefs.begin(kNamespace, false, kPartition);
    }
    if (!_ready)
    {
        Serial.printf("[warn] settings: NVS partition %s unavailable, config will not persist\n", kPartition);
        return false;
    }

    MigrateLegacy(_prefs);
    return true;
}

bool deskquad::SettingsStore::LoadWifi(WifiSettings &out)
{
    if (!_ready)
        return false;

    out.ssid = _prefs.getString(kWifiSsid, "");
    out.pass = _prefs.getString(kWifiPass, "");
    return out.ssid.length() > 0;
}

bool deskquad::SettingsStore::SaveWifi(const WifiSettings &value)
{
    if (!_ready)
        return false;

    bool ok = SaveString(_prefs, kWifiSsid, value.ssid);
    ok      = SaveString(_prefs, kWifiPass, value.pass) && ok;
    return ok;
}

bool deskquad::SettingsStore::LoadWeather(WeatherSettings &out)
{
    if (!_ready)
        return false;

    out.city   = _prefs.getString(kWeatherCity, "");
    out.apiKey = _prefs.getString(kWeatherApiKey, "");
    return out.apiKey.length() > 0;
}

bool deskquad::SettingsStore::SaveWeather(const WeatherSettings &value)
{
    if (!_ready)
        return false;

    bool ok = SaveString(_prefs, kWeatherCity, value.city);
    ok      = SaveString(_prefs, kWeatherApiKey, value.apiKey) && ok;
    return ok;
}

bool deskquad::SettingsStore::LoadAi(AiSettings &out)
{
    if (!_ready)
        return false;

    out.apiKey  = _prefs.getString(kAiKey, "");
    out.model   = _prefs.getString(kAiModel, "");
    out.baseUrl = _prefs.getString(kAiBaseUrl, "");
    return out.apiKey.length() > 0;
}

bool deskquad::SettingsStore::SaveAi(const AiSettings &value)
{
    if (!_ready)
        return false;

    bool ok = SaveString(_prefs, kAiKey, value.apiKey);
    ok      = SaveString(_prefs, kAiModel, value.model) && ok;
    ok      = SaveString(_prefs, kAiBaseUrl, value.baseUrl) && ok;
    return ok;
}

bool deskquad::SettingsStore::LoadVoice(VoiceSettings &out)
{
    if (!_ready)
        return false;

    out.apiKey = _prefs.getString(kVoiceApiKey, "");
    return out.apiKey.length() > 0;
}

bool deskquad::SettingsStore::SaveVoice(const VoiceSettings &value)
{
    if (!_ready)
        return false;

    return SaveString(_prefs, kVoiceApiKey, value.apiKey);
}
