#if !defined(APP_WAKEWORD_H)
#define APP_WAKEWORD_H

#include <memory>
#include <utility>

#include "app/AppFace.h"
#include "app/AppSettings.h"
#include "app/AppVoice.h"
#include "lib/WakeWordDetector.h"

/// wakeword (呼びかけ語) の常時待ち受け
class AppWakeWord {
public:
    explicit AppWakeWord(
            std::shared_ptr<AppSettings> settings,
            std::shared_ptr<AppVoice> voice,
            std::shared_ptr<AppFace> face
    ) : _settings(std::move(settings)),
        _voice(std::move(voice)),
        _face(std::move(face)) {};

    void setup();

    void start();

    /// WAV (16kHz/16bit/mono) から呼びかけ語を登録する
    /// @param append true: 既存に追加、false: 置き換え
    /// @return エラーメッセージ (nullptr: 成功)
    const char *registerWav(const uint8_t *data, size_t size, bool append);

    int templateWords();

    /// マイクの直近音量 (RMS, int16 振幅。マイク停止中は -1)
    int micLevel();

    /// 直近5秒間の最大音量
    int micPeak() { return (int) _lastPeak; }

    /// 直近の照合距離 (10秒以上照合が無ければ -1)
    int lastDistance();

    /// マイクから PCM を録音する (AppServer から呼ばれる)。成功時 true
    bool recordClip(int16_t *buf, size_t numSamples);

private:
    std::shared_ptr<AppSettings> _settings;
    std::shared_ptr<AppVoice> _voice;
    std::shared_ptr<AppFace> _face;

    TaskHandle_t _taskHandle{};

    SemaphoreHandle_t _lock = xSemaphoreCreateMutex();

    WakeWordDetector _detector;

    /// マイク使用中 (スピーカーとは排他)
    bool _micActive = false;

    /// 表情を戻す時刻 (0: 予約なし)
    unsigned long _resetFaceAt = 0;

    /// 音量ログの最終出力時刻
    unsigned long _lastLevelLog = 0;
    float _maxRms = 0;
    float _lastPeak = 0;

    /// 直近の照合結果
    int _lastDistance = -1;
    unsigned long _lastDistanceAt = 0;

    /// 録音要求 (recordClip と待ち受けタスクの受け渡し)
    volatile bool _captureRequested = false;
    volatile bool _captureDone = false;
    int16_t *_captureBuf = nullptr;
    size_t _captureLen = 0;
    size_t _capturePos = 0;

    void _loop();

    void _startMic();

    void _stopMic();

    void _onDetect();
};

#endif // !defined(APP_WAKEWORD_H)
