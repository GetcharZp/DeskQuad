#include <deskquad/hal/settings_store.h>

namespace
{

    constexpr const char *kNamespace = "deskquad";

    // NVS 键名最长 15 个字符
    constexpr const char *kWifiSsid      = "wifi_ssid";
    constexpr const char *kWifiPass      = "wifi_pass";
    constexpr const char *kWeatherCity   = "weather_city";
    constexpr const char *kWeatherApiKey = "weather_key";
    constexpr const char *kAiKey         = "ai_key";
    constexpr const char *kAiModel       = "ai_model";
    constexpr const char *kAiBaseUrl     = "ai_base_url";

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

    _ready = _prefs.begin(kNamespace, false);
    if (!_ready)
    {
        Serial.println("[warn] settings: NVS unavailable, config will not persist");
        return false;
    }

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
