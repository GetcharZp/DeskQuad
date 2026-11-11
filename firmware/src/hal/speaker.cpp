// speaker.cpp 旋律提示音实现
//
// 音符以方波合成，按 10ms 一块非阻塞写入 I2S 功放。功放同时被 TTS
// 使用，TTS 播放期间 IsToneBusy() 为 true，提示音自动让路。
#include <deskquad/hal/speaker.h>

namespace
{
    // 与 AudioIo 的功放配置保持一致：16kHz / 16bit / 单声道
    constexpr uint32_t kToneSampleRate = 16000;

    // 方波幅度约为满量程的 15%，避免提示音过响
    constexpr int16_t kToneAmplitude = 5000;

    // 每 10ms 喂一块，和 16kHz 采样率对应
    constexpr size_t kFeedSamples        = 160;
    constexpr unsigned long kFeedIntervalMs = 10;

    // Guard 在函数作用域内持有 Speaker 的递归锁，析构时释放
    class Guard
    {
    public:
        explicit Guard(SemaphoreHandle_t mutex) : _mutex(mutex)
        {
            if (_mutex != nullptr)
                xSemaphoreTakeRecursive(_mutex, portMAX_DELAY);
        }

        ~Guard()
        {
            if (_mutex != nullptr)
                xSemaphoreGiveRecursive(_mutex);
        }

    private:
        SemaphoreHandle_t _mutex;
    };
} // namespace

// 内置旋律定义

// 开机音效: 上行音阶
const Note Speaker::_startupMelody[] = {{NOTE_C4, 100},  {NOTE_E4, 100}, {NOTE_G4, 100}, {NOTE_C5, 150},
                                        {NOTE_REST, 50}, {NOTE_G4, 100}, {NOTE_C5, 200}, {NOTE_REST, 0}};

// 命令确认: 短促双音
const Note Speaker::_commandOkMelody[] = {{NOTE_E5, 80}, {NOTE_REST, 40}, {NOTE_G5, 120}, {NOTE_REST, 0}};

// 错误提示: 低沉下降
const Note Speaker::_errorMelody[] = {{NOTE_C4, 150},  {NOTE_REST, 50}, {NOTE_G3, 150},
                                      {NOTE_REST, 50}, {NOTE_E3, 200},  {NOTE_REST, 0}};

// 打招呼: 欢快三音
const Note Speaker::_helloMelody[] = {{NOTE_C5, 100}, {NOTE_E5, 100}, {NOTE_G5, 150}, {NOTE_REST, 0}};

// 再见: 下行
const Note Speaker::_goodbyeMelody[] = {{NOTE_G5, 120}, {NOTE_E5, 120}, {NOTE_C5, 200}, {NOTE_REST, 0}};

// 开心: 跳跃音
const Note Speaker::_happyMelody[] = {{NOTE_C5, 80}, {NOTE_D5, 80}, {NOTE_E5, 80},  {NOTE_C5, 80},
                                      {NOTE_E5, 80}, {NOTE_D5, 80}, {NOTE_C5, 160}, {NOTE_REST, 0}};

// 难过: 缓慢下降
const Note Speaker::_sadMelody[] = {{NOTE_G4, 200}, {NOTE_E4, 200}, {NOTE_C4, 300}, {NOTE_REST, 0}};

// 警告: 快速重复
const Note Speaker::_alertMelody[] = {{NOTE_C5, 100}, {NOTE_REST, 100}, {NOTE_C5, 100}, {NOTE_REST, 100},
                                      {NOTE_C5, 100}, {NOTE_REST, 100}, {NOTE_C5, 200}, {NOTE_REST, 0}};

// 完成: 下行三音
const Note Speaker::_doneMelody[] = {{NOTE_G5, 100}, {NOTE_E5, 100}, {NOTE_C5, 200}, {NOTE_REST, 0}};

// 简单哔声
const Note Speaker::_beepMelody[] = {{NOTE_C5, 200}, {NOTE_REST, 0}};

// 旋律指针映射表
const Note *Speaker::_melodyMap[] = {
    Speaker::_startupMelody,   // MELODY_STARTUP
    Speaker::_commandOkMelody, // MELODY_COMMAND_OK
    Speaker::_errorMelody,     // MELODY_ERROR
    Speaker::_helloMelody,     // MELODY_HELLO
    Speaker::_goodbyeMelody,   // MELODY_GOODBYE
    Speaker::_happyMelody,     // MELODY_HAPPY
    Speaker::_sadMelody,       // MELODY_SAD
    Speaker::_alertMelody,     // MELODY_ALERT
    Speaker::_doneMelody,      // MELODY_DONE
    Speaker::_beepMelody,      // MELODY_BEEP
};

// 初始化
void Speaker::begin()
{
    if (_mutex == nullptr)
        _mutex = xSemaphoreCreateRecursiveMutex();

    _mute = false;
    stop();
}

// 播放控制
void Speaker::play(Melody melody)
{
    if (_mute)
        return;
    // TTS 正在使用功放时丢弃提示音，避免两路 PCM 交错
    if (_sink != nullptr && _sink->IsToneBusy())
        return;

    if (melody < sizeof(_melodyMap) / sizeof(_melodyMap[0]))
    {
        const Note *notes = _melodyMap[melody];
        // 计算音符数量 (直到 freq=0)
        uint16_t count = 0;
        while (notes[count].freq != NOTE_REST || notes[count].duration > 0)
        {
            count++;
        }
        // 加 1 包含终止标记
        playNotes(notes, count + 1);
    }
}

void Speaker::playNotes(const Note *notes, uint16_t count)
{
    if (_mute || notes == nullptr || count == 0)
        return;
    if (_sink != nullptr && _sink->IsToneBusy())
        return;

    const Guard guard(_mutex);

    // 如果正在播放，先停止并清掉上一首的尾音
    stop();

    _currentNotes = notes;
    _noteCount    = count;
    _noteIndex    = 0;
    _isPlaying    = true;
    _noteStartMs  = millis();
    _lastFeedMs   = _noteStartMs;
    startNote(notes[0].freq);
}

void Speaker::stop()
{
    const Guard guard(_mutex);

    if (_sink != nullptr && !_sink->IsToneBusy())
        _sink->ClearTone();

    _isPlaying = false;
    // 保留 _currentNotes：语音任务可能并发调用 stop，清空指针会让主循环
    // 的 loop() 在空中解引用。播放状态统一由 _isPlaying 判断。
    _noteCount = 0;
    _noteIndex = 0;
    _phase     = 0;
    _phaseStep = 0;
}

uint16_t Speaker::CurrentNoteFreq() const
{
    const Guard guard(_mutex);
    if (!_isPlaying || _currentNotes == nullptr || _noteIndex >= _noteCount)
        return 0;
    return _currentNotes[_noteIndex].freq;
}

void Speaker::startNote(uint16_t freq)
{
    // freq 为 0 表示休止符：相位步进为 0，feedTone 输出静音
    _phase     = 0;
    _phaseStep = freq == 0 ? 0 : static_cast<uint32_t>((static_cast<uint64_t>(freq) << 32) / kToneSampleRate);

    // 丢掉上一音符还留在 DMA 里的数据，让换音立即生效
    if (_sink != nullptr && !_sink->IsToneBusy())
        _sink->ClearTone();
}

// 主循环 (非阻塞播放更新)
void Speaker::loop()
{
    const Guard guard(_mutex);

    if (!_isPlaying || _currentNotes == nullptr)
        return;

    const unsigned long now     = millis();
    const unsigned long elapsed = now - _noteStartMs;

    // 计算当前应该播放到哪个音符
    unsigned long accumulated = 0;
    uint16_t index            = _noteCount;
    for (uint16_t i = 0; i < _noteCount; i++)
    {
        const unsigned long noteEnd = accumulated + _currentNotes[i].duration;
        if (elapsed < noteEnd)
        {
            index = i;
            break;
        }
        accumulated = noteEnd;
    }

    // 播放完毕
    if (index >= _noteCount)
    {
        _isPlaying    = false;
        _currentNotes = nullptr;
        _noteCount    = 0;
        _noteIndex    = 0;
        _phaseStep    = 0;
        return;
    }

    if (index != _noteIndex)
    {
        _noteIndex = index;
        startNote(_currentNotes[index].freq);
    }

    if (_sink == nullptr || _sink->IsToneBusy())
        return;

    // 按固定节拍喂数据，避免主循环跑得越快写得越多
    if (now - _lastFeedMs < kFeedIntervalMs)
        return;
    _lastFeedMs = now;
    feedTone();
}

void Speaker::feedTone()
{
    int16_t samples[kFeedSamples];
    for (size_t i = 0; i < kFeedSamples; i++)
    {
        if (_phaseStep == 0)
        {
            samples[i] = 0;
        }
        else
        {
            _phase += _phaseStep;
            // 相位最高位决定方波高低电平
            samples[i] = (_phase & 0x80000000u) != 0 ? kToneAmplitude : -kToneAmplitude;
        }
    }
    _sink->WriteTone(samples, kFeedSamples);
}
