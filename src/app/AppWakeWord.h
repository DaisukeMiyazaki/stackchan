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

    void _loop();

    void _startMic();

    void _stopMic();

    void _onDetect();
};

#endif // !defined(APP_WAKEWORD_H)
