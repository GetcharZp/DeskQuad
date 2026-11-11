#include <deskquad/voice/audio_io.h>

#include <driver/i2s.h>
#include <esp_intr_alloc.h>

#include <deskquad/config/pins.h>
#include <deskquad/voice/voice_config.h>

namespace
{
    constexpr i2s_port_t kMicPort = I2S_NUM_0;
    constexpr i2s_port_t kAmpPort = I2S_NUM_1;

    constexpr int kDmaCount = 6;
    constexpr int kDmaLen   = 320; // 20ms @ 16kHz

    i2s_config_t micConfig()
    {
        i2s_config_t cfg        = {};
        cfg.mode                = static_cast<i2s_mode_t>(I2S_MODE_MASTER | I2S_MODE_RX);
        cfg.sample_rate         = deskquad::voice::kSampleRate;
        cfg.bits_per_sample     = I2S_BITS_PER_SAMPLE_32BIT;
        cfg.channel_format      = I2S_CHANNEL_FMT_RIGHT_LEFT;
        cfg.communication_format = I2S_COMM_FORMAT_STAND_I2S;
        cfg.intr_alloc_flags    = ESP_INTR_FLAG_LEVEL1;
        cfg.dma_buf_count       = kDmaCount;
        cfg.dma_buf_len         = kDmaLen;
        cfg.use_apll            = false;
        cfg.tx_desc_auto_clear  = false;
        return cfg;
    }

    i2s_config_t ampConfig()
    {
        i2s_config_t cfg        = {};
        cfg.mode                = static_cast<i2s_mode_t>(I2S_MODE_MASTER | I2S_MODE_TX);
        cfg.sample_rate         = deskquad::voice::kSampleRate;
        cfg.bits_per_sample     = I2S_BITS_PER_SAMPLE_16BIT;
        cfg.channel_format      = I2S_CHANNEL_FMT_ONLY_LEFT;
        cfg.communication_format = I2S_COMM_FORMAT_STAND_I2S;
        cfg.intr_alloc_flags    = ESP_INTR_FLAG_LEVEL1;
        cfg.dma_buf_count       = kDmaCount;
        cfg.dma_buf_len         = kDmaLen;
        cfg.use_apll            = false;
        cfg.tx_desc_auto_clear  = true;
        return cfg;
    }
} // namespace

bool AudioIo::Begin()
{
    const i2s_config_t mic = micConfig();
    if (i2s_driver_install(kMicPort, &mic, 0, nullptr) != ESP_OK)
    {
        Serial.println("[warn] voice: mic i2s install failed");
        return false;
    }

    i2s_pin_config_t micPins = {};
    micPins.mck_io_num       = I2S_PIN_NO_CHANGE;
    micPins.bck_io_num       = deskquad::kMicI2sBclkPin;
    micPins.ws_io_num        = deskquad::kMicI2sWsPin;
    micPins.data_out_num     = I2S_PIN_NO_CHANGE;
    micPins.data_in_num      = deskquad::kMicI2sDinPin;
    if (i2s_set_pin(kMicPort, &micPins) != ESP_OK)
    {
        Serial.println("[warn] voice: mic i2s pin failed");
        i2s_driver_uninstall(kMicPort);
        return false;
    }

    const i2s_config_t amp = ampConfig();
    if (i2s_driver_install(kAmpPort, &amp, 0, nullptr) != ESP_OK)
    {
        Serial.println("[warn] voice: amp i2s install failed");
        i2s_driver_uninstall(kMicPort);
        return false;
    }

    i2s_pin_config_t ampPins = {};
    ampPins.mck_io_num       = I2S_PIN_NO_CHANGE;
    ampPins.bck_io_num       = deskquad::kAmpI2sBclkPin;
    ampPins.ws_io_num        = deskquad::kAmpI2sLrcPin;
    ampPins.data_out_num     = deskquad::kAmpI2sDinPin;
    ampPins.data_in_num      = I2S_PIN_NO_CHANGE;
    if (i2s_set_pin(kAmpPort, &ampPins) != ESP_OK)
    {
        Serial.println("[warn] voice: amp i2s pin failed");
        i2s_driver_uninstall(kMicPort);
        i2s_driver_uninstall(kAmpPort);
        return false;
    }

    i2s_zero_dma_buffer(kAmpPort);
    _ready = true;
    return true;
}

size_t AudioIo::ReadMic(int16_t *dst, size_t maxSamples, uint32_t timeoutMs)
{
    if (!_ready || dst == nullptr || maxSamples == 0)
        return 0;

    const size_t want = maxSamples > 320 ? 320 : maxSamples;
    const size_t bytes = want * 2 * sizeof(int32_t);
    size_t read       = 0;
    const TickType_t timeout = pdMS_TO_TICKS(timeoutMs == 0 ? 1 : timeoutMs);
    if (i2s_read(kMicPort, _raw, bytes, &read, timeout) != ESP_OK || read == 0)
        return 0;

    const size_t count = read / (2 * sizeof(int32_t));
    for (size_t i = 0; i < count; i++)
    {
        // INMP441 的 24bit 数据左对齐在 32bit 槽里，取高 16bit 即 16bit PCM
        dst[i] = static_cast<int16_t>(_raw[i * 2] >> 16);
    }
    return count;
}

bool AudioIo::WritePcm(const int16_t *src, size_t samples)
{
    if (!_ready || src == nullptr || samples == 0)
        return false;

    constexpr size_t kChunk = 320; // 20ms
    size_t offset           = 0;
    while (offset < samples)
    {
        const size_t count = samples - offset > kChunk ? kChunk : samples - offset;
        const size_t bytes = count * sizeof(int16_t);
        size_t sent        = 0;
        while (sent < bytes)
        {
            size_t written = 0;
            if (i2s_write(kAmpPort, reinterpret_cast<const uint8_t *>(src + offset) + sent, bytes - sent, &written,
                          pdMS_TO_TICKS(100)) != ESP_OK)
                return false;
            if (written == 0)
                return false;
            sent += written;
        }
        offset += count;
    }
    return true;
}

void AudioIo::ClearSpeaker()
{
    if (_ready)
        i2s_zero_dma_buffer(kAmpPort);
}

size_t AudioIo::WriteTone(const int16_t *samples, size_t count)
{
    if (!_ready || _ttsActive || samples == nullptr || count == 0)
        return 0;

    // ticks_to_wait = 0：非阻塞。DMA 满时直接丢弃这一块，提示音不需要精确补帧。
    size_t written = 0;
    if (i2s_write(kAmpPort, samples, count * sizeof(int16_t), &written, 0) != ESP_OK)
        return 0;
    return written / sizeof(int16_t);
}

void AudioIo::ClearTone()
{
    // TTS 正在播放时不能清 DMA，否则会截断语音
    if (_ready && !_ttsActive)
        ClearSpeaker();
}
